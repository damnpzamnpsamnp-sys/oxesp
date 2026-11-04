#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define LOG_TAG "oxesp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace cfg {
    constexpr int   kEntityCount  = 128;
    constexpr int   kEntityStride = 8;
    constexpr float kHeadHeight   = 1.8f;
    constexpr float kBoxWidthCoef = 0.5f;
}

namespace off {
    constexpr uintptr_t kViewMatrixPtrRva = 0x0;
    constexpr uintptr_t kLocalPlayerRva   = 0x0;
    constexpr uintptr_t kEntityListRva    = 0x0;
    constexpr uintptr_t kLocalPosOffset   = 0x0;
    constexpr uintptr_t kLocalTeamOffset  = 0x0;
    constexpr uintptr_t kEntPosOffset     = 0x0;
    constexpr uintptr_t kEntTeamOffset    = 0x0;
    constexpr uintptr_t kEntHealthOffset  = 0x0;
}

struct Vec3 { float x, y, z; };

static int       g_memFd   = -1;
static int       g_pid     = 0;
static uintptr_t g_il2cpp  = 0;
static int       g_sw      = 0;
static int       g_sh      = 0;
static std::vector<float> g_out;

static bool ReadMem(uintptr_t addr, void* out, size_t n) {
    if (g_memFd < 0) return false;
    return pread64(g_memFd, out, n, (off_t)addr) == (ssize_t)n;
}
template <typename T>
static bool ReadT(uintptr_t addr, T& v) { return ReadMem(addr, &v, sizeof(T)); }

static int FindPidByPackage(const std::string& pkg) {
    DIR* d = opendir("/proc");
    if (!d) return 0;
    dirent* e;
    int found = 0;
    while ((e = readdir(d))) {
        if (e->d_type != DT_DIR) continue;
        char* end = nullptr;
        long pid = strtol(e->d_name, &end, 10);
        if (*end || pid <= 0) continue;
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        char buf[256] = {};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0 && pkg == buf) { found = (int)pid; break; }
    }
    closedir(d);
    return found;
}

static uintptr_t FindIl2CppBase(int pid) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    uintptr_t best = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "libil2cpp.so")) continue;
        uintptr_t start = strtoull(line, nullptr, 16);
        if (best == 0 || start < best) best = start;
    }
    fclose(f);
    return best;
}

static bool WorldToScreen(const float* m, const Vec3& w, float& sx, float& sy) {
    float cx = m[0]*w.x + m[4]*w.y + m[8] *w.z + m[12];
    float cy = m[1]*w.x + m[5]*w.y + m[9] *w.z + m[13];
    float cw = m[3]*w.x + m[7]*w.y + m[11]*w.z + m[15];
    if (cw < 0.001f) return false;
    float inv = 1.f / cw;
    float ndcX = cx * inv;
    float ndcY = cy * inv;
    sx = (float)g_sw * 0.5f * (ndcX + 1.f);
    sy = (float)g_sh * 0.5f * (1.f - ndcY);
    return true;
}

static void TickImpl() {
    g_out.clear();
    if (!g_il2cpp || g_memFd < 0) return;

    uintptr_t vmPtr = 0;
    float vm[16];
    if (!ReadT(g_il2cpp + off::kViewMatrixPtrRva, vmPtr) || !vmPtr) return;
    if (!ReadMem(vmPtr, vm, sizeof(vm))) return;

    uintptr_t localPlayer = 0;
    if (!ReadT(g_il2cpp + off::kLocalPlayerRva, localPlayer) || !localPlayer) return;

    Vec3 localPos{};
    if (!ReadMem(localPlayer + off::kLocalPosOffset, &localPos, sizeof(localPos))) return;

    int localTeam = 0;
    ReadT(localPlayer + off::kLocalTeamOffset, localTeam);

    uintptr_t listPtr = 0;
    if (!ReadT(g_il2cpp + off::kEntityListRva, listPtr) || !listPtr) return;

    for (int i = 0; i < cfg::kEntityCount; ++i) {
        uintptr_t ent = 0;
        if (!ReadT(listPtr + (uintptr_t)i * cfg::kEntityStride, ent) || !ent) continue;
        if (ent == localPlayer) continue;

        Vec3 pos{};
        int team = 0;
        float hp = 0.f;
        if (!ReadMem(ent + off::kEntPosOffset, &pos, sizeof(pos))) continue;
        if (!ReadT(ent + off::kEntTeamOffset, team)) continue;
        if (!ReadT(ent + off::kEntHealthOffset, hp)) continue;
        if (team == localTeam) continue;
        if (hp <= 0.f) continue;

        float fx, fy, hx, hy;
        if (!WorldToScreen(vm, pos, fx, fy)) continue;
        Vec3 headPos{ pos.x, pos.y + cfg::kHeadHeight, pos.z };
        if (!WorldToScreen(vm, headPos, hx, hy)) continue;

        float bh = fy - hy;
        if (bh < 4.f || bh > (float)g_sh) continue;
        float bw = bh * cfg::kBoxWidthCoef;

        float x1 = hx - bw * 0.5f;
        float x2 = hx + bw * 0.5f;
        float dx = pos.x - localPos.x;
        float dy = pos.y - localPos.y;
        float dz = pos.z - localPos.z;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

        g_out.push_back(x1);
        g_out.push_back(hy);
        g_out.push_back(x2);
        g_out.push_back(fy);
        g_out.push_back(dist);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_jb2ai_oxidesp_NativeBridge_init(JNIEnv* env, jclass, jstring pkg, jint sw, jint sh) {
    if (g_memFd >= 0) { close(g_memFd); g_memFd = -1; }
    g_out.clear();
    g_il2cpp = 0;
    g_pid = 0;

    const char* cstr = env->GetStringUTFChars(pkg, nullptr);
    std::string package(cstr ? cstr : "");
    if (cstr) env->ReleaseStringUTFChars(pkg, cstr);

    g_pid = FindPidByPackage(package);
    if (!g_pid) { LOGE("package %s not running", package.c_str()); return JNI_FALSE; }

    char memPath[64];
    std::snprintf(memPath, sizeof(memPath), "/proc/%d/mem", g_pid);
    g_memFd = open(memPath, O_RDONLY | O_CLOEXEC);
    if (g_memFd < 0) { LOGE("open %s failed (root?)", memPath); return JNI_FALSE; }

    g_il2cpp = FindIl2CppBase(g_pid);
    if (!g_il2cpp) { LOGE("libil2cpp.so not found in maps"); close(g_memFd); g_memFd = -1; return JNI_FALSE; }

    g_sw = sw;
    g_sh = sh;

    LOGI("init ok pid=%d il2cpp=0x%lx screen=%dx%d", g_pid, (unsigned long)g_il2cpp, g_sw, g_sh);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_jb2ai_oxidesp_NativeBridge_tick(JNIEnv* env, jclass) {
    TickImpl();
    jfloatArray arr = env->NewFloatArray((jsize)g_out.size());
    if (arr && !g_out.empty()) {
        env->SetFloatArrayRegion(arr, 0, (jsize)g_out.size(), g_out.data());
    }
    return arr ? arr : env->NewFloatArray(0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_jb2ai_oxidesp_NativeBridge_shutdown(JNIEnv*, jclass) {
    if (g_memFd >= 0) { close(g_memFd); g_memFd = -1; }
    g_il2cpp = 0;
    g_pid = 0;
    g_out.clear();
}
