#include "MumbleLink.h"
#include <cassert>
#include <cstring>

#ifdef WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace spades {
    MumbleLink::MumbleLink() : metre_per_block(0.63), mumbleLinkedMemory(nullptr) {}

    MumbleLink::~MumbleLink() {
    #ifdef WIN32
      UnmapViewOfFile(mumbleLinkedMemory);
      if (obj != nullptr)
        CloseHandle(obj);
    #else
      munmap(mumbleLinkedMemory, sizeof(*mumbleLinkedMemory));
      if (fd > 0)
        close(fd);
    #endif
    }

    void MumbleLink::set_mumble_vector3(float mumble_vec[3],
                                        const spades::Vector3 &vec) {
      mumble_vec[0] = vec.x;
      mumble_vec[1] = vec.z;
      mumble_vec[2] = vec.y;
    }

    bool MumbleLink::init() {
      assert(mumbleLinkedMemory == nullptr);
    #ifdef WIN32
      obj = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
      if (obj == nullptr)
        return false;

      mumbleLinkedMemory = static_cast<MumbleLinkedMemory *>(MapViewOfFile(
          obj, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*mumbleLinkedMemory)));

      if (mumbleLinkedMemory == nullptr) {
        CloseHandle(obj);
        obj = nullptr;
        return false;
      }
    #else
      std::string name = "/MumbleLink." + std::to_string(getuid());

      fd = shm_open(name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);

      if (fd < 0) {
        return false;
      }

      mumbleLinkedMemory = static_cast<MumbleLinkedMemory *>(
          (mmap(nullptr, sizeof(*mumbleLinkedMemory), PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0)));

      if (mumbleLinkedMemory == MAP_FAILED) {
        mumbleLinkedMemory = nullptr;
        return false;
      }
    #endif
      return true;
    }

    void MumbleLink::setContext(const std::string &context) {
      if (mumbleLinkedMemory == nullptr)
        return;
      size_t len(std::min(256, static_cast<int>(context.size())));
      std::memcpy(mumbleLinkedMemory->context, context.c_str(), len);
      mumbleLinkedMemory->context_len = len;
    }

    void MumbleLink::setIdentity(const std::string &identity) {
      if (mumbleLinkedMemory == nullptr)
        return;
      std::wcsncpy(mumbleLinkedMemory->identity,
                   std::wstring(identity.begin(), identity.end()).c_str(), 256);
    }

    void MumbleLink::update(spades::client::Player *player) {
      if (mumbleLinkedMemory == nullptr or player == nullptr)
        return;

      if (mumbleLinkedMemory->uiVersion != 2) {
        wcsncpy(mumbleLinkedMemory->name, L"OpenSpades", 256);
        wcsncpy(mumbleLinkedMemory->description, L"OpenSpades Link plugin.", 2048);
        mumbleLinkedMemory->uiVersion = 2;
      }
      mumbleLinkedMemory->uiTick++;

      // Left handed coordinate system.
      // X positive towards "right".
      // Y positive towards "up".
      // Z positive towards "front".
      //
      // 1 unit = 1 meter

      // Unit vector pointing out of the avatar's eyes aka "At"-vector.
      set_mumble_vector3(mumbleLinkedMemory->fAvatarFront, player->GetFront());

      // Unit vector pointing out of the top of the avatar's head aka "Up"-vector
      // (here Top points straight up).
      set_mumble_vector3(mumbleLinkedMemory->fAvatarTop, player->GetUp());

      // Position of the avatar (here standing slightly off the origin)
      set_mumble_vector3(mumbleLinkedMemory->fAvatarPosition,
                         player->GetPosition() * metre_per_block);

      // Same as avatar but for the camera.
      set_mumble_vector3(mumbleLinkedMemory->fCameraPosition,
                         player->GetPosition() * metre_per_block);
      set_mumble_vector3(mumbleLinkedMemory->fCameraFront, player->GetFront());
      set_mumble_vector3(mumbleLinkedMemory->fCameraTop, player->GetUp());
    }
}
