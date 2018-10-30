// vi:fo=qacj com=b\://

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include <complex>
#include <cmath>
using namespace std;

#include <GL/glew.h>  // needed for shaders and shit.
#include <GLFW/glfw3.h>

#include "helpers.hpp"


// Computes x^n, where n is a natural number.
float pown(float x, unsigned n)
{
    float y = 1;
    // n = 2*d + r. x^n = (x^2)^d * x^r.
    unsigned d = n >> 1;
    unsigned r = n & 1;
    float x_2_d = d == 0? 1 : pown(x*x, d);
    float x_r = r == 0? 1 : x;
    return x_2_d*x_r;
}
// The linear implementation.
float pown_l(float x, unsigned n)
{
    float y = 1;
    for (unsigned i = 0; i < n; i++)
        y *= x;
    return y;
}

// factorials
float fact(unsigned n)
{
    float m = 1;
    for (unsigned i = n; i; i--)
        m *= i;
    return m;
}

float sq(float x)
{
    return x*x;
}

float modsq(complex<float> x)
{
    return sq(x.real()) + sq(x.imag());
}

// Read a file to a string.
char *load(const char *fn)
{
    int fd = open(fn, O_RDONLY);
    assert(fd != -1);

    off_t size = lseek(fd, 0, SEEK_END);
    assert(size != -1);
    assert(lseek(fd, 0, SEEK_SET) != -1);

    size++;  // null terminator
    char *buf = (char *)malloc(size);

    char *p = buf;
    for (;;) {
        // File has gotten bigger since we started? Fuck that.
        assert(p - buf < size);
        ssize_t nread = read(fd, (char *)p, 0x10000);
        assert(nread != -1);
        if (nread == 0) {
            *p = '\0';
            break;
        }

        // Null character? Fuck that shit.
        void *nullbyte = memchr((char *)p, '\0', nread);
        assert(nullbyte == NULL);

        p += nread;
    }
    assert(close(fd) == 0);

    return buf;
}

// Do not call this function directly. Use the macro gl_assert(). Use it after
// an OpenGL API call to abort with a helpful error message if anything goes
// wrong. Example:
/// glCompileShader(shader); gl_assert();
void _gl_assert(const char *file, unsigned int line, const char *function)
{
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        fprintf(stderr, "GL assertion failed. file: %s\n"
                "function: %s\nline: %d     GL error: %s\n",
                file, function, line, gluErrorString(e));
        abort();
    }
}

// Compile the "type" shader named "filename" and attach it to
// "shader_program".
static void compile_shader(GLenum type, const char *filename,
        GLuint shader_program)
{
    GLuint shader = glCreateShader(type);
    char *source = load(filename);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);
#ifndef NDEBUG
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        char *log = (char *)malloc(log_length);
        glGetShaderInfoLog(shader, log_length, NULL, log);
        fprintf(stderr, "Failed to compile %s:\n%s", filename, log);
        abort();
    }
#endif
    glAttachShader(shader_program, shader);
}

// Compile the vertex shader named "vertfile" and the fragment shader named
// "fragfile". Attach both to shader_program.
void compile_shaders(const char *vertfile, const char *fragfile,
        GLuint shader_program)
{
    compile_shader(GL_VERTEX_SHADER, vertfile, shader_program);
    compile_shader(GL_FRAGMENT_SHADER, fragfile, shader_program);
}

complex<float> *linspacecf(complex<float> a, complex<float> b, unsigned N)
{
    DEF_ARRAY(complex<float>, y, N);
    for (unsigned u = 0; u < N; u++) {
        y[u] = a + (b - a)/(float)(N - 1) * (float)u;
    }
    return y;
}

void line_strip_to_lines(complex<float> *x, unsigned n,
        complex<float> **py, unsigned *pny)
{
    assert(n >= 2);
    unsigned ny = 2*n - 2;
    DEF_ARRAY(complex<float>, y, ny);
    for (unsigned i = 0; i < n - 1; i++) {
        y[2*i] = x[i];
        y[2*i + 1] = x[i + 1];
    }
    *py = y;
    *pny = ny;
}

void d_to_timespec(double t, struct timespec *ts)
{
    ts->tv_sec = (time_t)(floor(t));
    ts->tv_nsec = (long)(t*1e9) % (1000*1000*1000);
}

double timespec_to_d(struct timespec *ts)
{
    return (double)ts->tv_sec + 1e-9*(double)ts->tv_nsec;
}

double dtime(void)
{
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return timespec_to_d(&ts);
}

void dsleep(double t)
{
    struct timespec ts;
    d_to_timespec(t, &ts);
    assert(clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL) == 0);
}


timer_t create_callback_timer(void (*callback)(void *), void *data)
{
    struct sigevent se;
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_signo = 0;
    se.sigev_value.sival_ptr = data;
    se.sigev_notify_function = (void (*)(union sigval))callback;
    se.sigev_notify_attributes = NULL;

    timer_t timer;

    assert(timer_create(CLOCK_MONOTONIC, &se, &timer) == 0);

    return timer;
}
void timer_settime_d(timer_t timer, double t)
{
    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    d_to_timespec(t, &its.it_value);
    assert(timer_settime(timer, 0, &its, NULL) == 0);
}
