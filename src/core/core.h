#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#include <chrono>

#if defined(BNG_IS_WINDOWS)
# include <direct.h>
# if !defined(chdir)
#   define chdir _chdir
# endif
# if !defined(unlink)
#   define unlink _unlink
# endif
# pragma warning( disable : 4514 5045 )
# if defined(BNG_DEBUG)
extern "C" { __declspec(dllimport) void __stdcall OutputDebugStringA(const char*); }
#   define BNG_PUTI(S) do { const char* s = S; fputs(s, stdout); OutputDebugStringA(s); } while(0)
#   define BNG_PUTE(S) do { const char* s = S; fputs(s, stderr); OutputDebugStringA(s); } while(0)
# endif
#else
# include <unistd.h>
using rsize_t = uint32_t;
inline void strcpy_s(char* dst, rsize_t max_count, const char* src) {
  (void)max_count;
  strcpy(dst, src);
}
inline bool fopen_s(FILE** pfp, const char* path, const char* mode) {
  *pfp = fopen(path, mode);
  return !!*pfp;
}
#define sprintf_s(DST, MAX_COUNT, FMT, ...) sprintf(DST, FMT, ##__VA_ARGS__)
#endif

#if !defined(BNG_PUTI)
# define BNG_PUTI(S) do { const char* s = S; fputs(s, stdout); } while(0)
# define BNG_PUTE(S) do { const char* s = S; fputs(s, stderr); } while(0)
#endif

#define BNG_DECL_NO_COPY(CLASS) \
  CLASS(const CLASS&) = delete; \
  CLASS& operator =(const CLASS&) = delete;\
  CLASS(CLASS&& rhs) noexcept { \
    memcpy(this, &rhs, sizeof(*this)); \
    memset(&rhs, 0, sizeof(*this)); \
  }; \
  CLASS& operator =(CLASS&& rhs) noexcept { \
    this->~CLASS(); \
    memcpy(this, &rhs, sizeof(*this)); \
    memset(&rhs, 0, sizeof(*this)); \
    return *this; \
  }

#define BNG_STRINGIFY(V) #V

#define BNG_LOGI(FMT, ...) \
  do { \
    char log_line[1024]; \
    sprintf_s(log_line, sizeof(log_line), "(%d): " FMT "\n", __LINE__, ##__VA_ARGS__); \
    BNG_PUTI(bng::core::log::basename(__FILE__)); \
    BNG_PUTI(log_line); fflush(stdout); \
  } while(0)

#define BNG_LOGE(FMT, ...) \
  do { \
    char log_line[1024]; \
    sprintf_s(log_line, sizeof(log_line), "(%d): " FMT "\n", __LINE__, ##__VA_ARGS__); \
    BNG_PUTE(bng::core::log::basename(__FILE__)); \
    BNG_PUTE(log_line); fflush(stderr); \
  } while(0)

#define BNG_PRINT(FMT, ...) \
  do { \
    char log_line[1024]; \
    sprintf_s(log_line, sizeof(log_line), FMT, ##__VA_ARGS__); \
    BNG_PUTI(log_line); fflush(stdout); \
  } while(0)

#if defined(BNG_DEBUG)
# define BNG_VERIFY(COND, FMT, ...) \
  do { \
    auto check_fail = !(COND); \
    if (check_fail) { \
      BNG_LOGE(#COND " is false. " FMT, ##__VA_ARGS__); \
      assert(false); \
    } \
  } while(0)
#else
# define BNG_VERIFY(...) do { } while(0)
#endif

namespace bng::core {
  namespace log {
    inline const char* basename(const char* file) {
      for (const char* p = file; *p; p++) {
        if (*p == '\\' || *p == '/') {
          file = p + 1;
        }
      }
      return file;
    }
  }
  using clock = std::chrono::steady_clock;

  class ScopedTimer {
  public:
    BNG_DECL_NO_COPY(ScopedTimer);

    enum class Units : uint32_t { ns, us, ms, s };

    ScopedTimer() 
      : start(clock::now())
    {}

    explicit ScopedTimer(const char* file, uint32_t line, const char* msg, Units units = Units::ms)
      : start(clock::now()), msg(msg), file(log::basename(file)), units(units), line(line)
    {
    }

    ~ScopedTimer() {
      if (msg) {
        char time_buf[64];
        char num_buf[32];
        sprintf_s(time_buf, sizeof(time_buf), " [%0.3lf%s]\n", elapsed(units), units_sfx(units));
        sprintf_s(num_buf, sizeof(num_buf), "(%d): ", line);
        BNG_PUTI(file);
        BNG_PUTI(num_buf);
        BNG_PUTI(msg);
        BNG_PUTI(time_buf);
      }
    }

    double elapsed(Units u) const {
      switch (u) {
      case Units::ns: return elapsed_ns();
      case Units::us: return elapsed_us();
      case Units::ms: return elapsed_ms();
      case Units::s: return elapsed_s();
      }
      return 0.0;
    }

    static const char* units_sfx(Units u) {
      static const char* sym[] = { "ns", "us", "ms", "s" };
      return (u <= Units::s) ? sym[uint32_t(u)] : "ER";
    }

    double elapsed_ns() const {
      return double(elapsed_dur().count());
    }

    double elapsed_us() const {
      return elapsed_ns() / 1000.0;
    }

    double elapsed_ms() const {
      return elapsed_ns() / 1000000.0;
    }

    double elapsed_s() const {
      return elapsed_ns() / 1000000000.0;
    }

  private:
    clock::duration elapsed_dur() const {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start);
    }

  private:
    const clock::time_point start;
    const char* msg = nullptr;
    const char* file = nullptr;
    Units units = Units::ms;
    uint32_t line = 0;
  };

#define BNG_SCOPED_TIMER(MSG, ...) \
  ScopedTimer(__FILE__, __LINE__, MSG, ##__VA_ARGS__)

  struct File {
    BNG_DECL_NO_COPY(File);
    FILE* fp = nullptr;

    explicit File(const char* path, const char* mode) {
      fopen_s(&fp, path, mode);
    }

    operator FILE* () { return fp; }
    operator bool() const { return !!fp; }
    bool operator!() const { return !fp; }

    uint32_t size_bytes() {
      fseek(fp, 0, SEEK_END);
      auto sz = uint32_t(ftell(fp));
      rewind(fp);
      return sz;
    }

    ~File() {
      if (fp) {
        fclose(fp);
        fp = nullptr;
      }
    }
  };
} // namespace bng

