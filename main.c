#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define WINDOW_WIDTH    500
#define WINDOW_HEIGHT   500
#define WINDOW_TITLE    "GL Simple Test"

static int running = 0;

static void signal_int(int signum)
{
        running = 0;
}

void xwindow_init(Display **display, EGLNativeWindowType *window)
{
    Window rootWindow, xWindow;
    XSetWindowAttributes attrs;

    *display = XOpenDisplay(NULL);
    assert(display);

    attrs.event_mask = ExposureMask;
    rootWindow = DefaultRootWindow(*display);
    xWindow = XCreateWindow(*display, rootWindow, 0, 0, WINDOW_WIDTH,
                            WINDOW_HEIGHT, 0, CopyFromParent, InputOutput,
                            CopyFromParent, CWEventMask, &attrs);
    XMapWindow(*display, xWindow);
    XStoreName(*display, xWindow, WINDOW_TITLE);

    *window = (EGLNativeWindowType)xWindow;
}

void egl_init(EGLNativeWindowType window, EGLDisplay eglDisplay,
	      EGLContext *eglContext, EGLSurface *eglSurface)
{
    EGLBoolean ret;
    EGLint major, minor, n;
    EGLint attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_ALPHA_SIZE, 1,
	EGL_DEPTH_SIZE, 16,
	EGL_RED_SIZE, 1,
	EGL_GREEN_SIZE, 1,
	EGL_BLUE_SIZE, 1,
        EGL_NONE
    };
    EGLConfig config;
    EGLDisplay dpy;
    EGLSurface surf;
    EGLContext context;
    EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    dpy = eglDisplay;
    assert(dpy);

    ret = eglInitialize(dpy, &major, &minor);
    assert(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
    assert(ret == EGL_TRUE);

    ret = eglChooseConfig(dpy, attribs, &config, 1, &n);
    assert(ret && n == 1);
    context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attrs);
    assert(context);

    eglMakeCurrent(dpy, NULL, NULL, context);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    surf = eglCreateWindowSurface(dpy, config, window, NULL);
    assert(surf);

    eglMakeCurrent(dpy, surf, surf, context);

    *eglContext = context;
    *eglSurface = surf;
}

void egl_deinit(EGLDisplay eglDisplay, EGLSurface eglSurface)
{
    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(eglDisplay, eglSurface);
    eglTerminate(eglDisplay);
}

#define TO_STRING(str)	#str
static const char *vshader_code = TO_STRING(
    attribute vec3 position;
    attribute vec2 texcoord;
    varying vec2 v_texcoord;
    void main()
    {
	gl_Position = vec4(position, 1.0);
	v_texcoord = texcoord;
    });
static const char *fshader_code = TO_STRING(
    precision mediump float;
    varying vec2 v_texcoord;
    uniform sampler2D texture;
    void main()
    {
	gl_FragColor = texture2D(texture, v_texcoord);
    });

static GLuint compile_shader(const char *src, GLenum type)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(type);
    if (!shader) {
	fprintf(stderr, "Can't create shader\n");
	return 0;
    }

    glShaderSource(shader, 1, (const char **)&src, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
	char log[1024];
	GLsizei len;
	glGetShaderInfoLog(shader, 1024, &len, log);
	fprintf(stderr, "Error: compiling %s: %*s\n",
		type == GL_VERTEX_SHADER ? "vertex" : "fragment",
		len, log);
	return 0;
    }

    return shader;
}

static GLuint build_shader(void)
{
    GLuint vshader, fshader, program;
    GLint status;

    vshader = compile_shader(vshader_code, GL_VERTEX_SHADER);
    if (!vshader)
	return 0;
    fshader = compile_shader(fshader_code, GL_FRAGMENT_SHADER);
    if (!fshader)
	return 0;

    program = glCreateProgram();
    glAttachShader(program, vshader);
    glAttachShader(program, fshader);
    glLinkProgram(program);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
	char log[1024];
	GLsizei len;
	glGetProgramInfoLog(program, 1024, &len ,log);
	fprintf(stderr, "Error: linking:\n%*s\n", len, log);
	return 0;
    }

    return program;
}

#define R	128
static GLuint create_texture(void)
{
    GLuint texture;
    uint32_t data[R * 2 * R * 2] = {0};
    uint32_t *p = data;
    int x, y;

    for (y = 0; y < R * 2; y++) {
	for (x = 0; x < R * 2; x++) {
	    if ((x - R) * (x - R) + (y - R) * (y - R) < R * R)
		*p = 0xffff0000;
	    p++;
	}
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, R * 2, R * 2, 0, GL_RGBA,
		 GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

static GLuint texture;
static GLuint program;
static GLuint sh_loc_position, sh_loc_texcoord, sh_loc_texture;
static GLuint vertex_buf, texcoord_buf, index_buf;

static int gl_init(void)
{
    program = build_shader();
    if (!program) {
	fprintf(stderr, "fail to build shader\n");
	return -1;
    }

    glUseProgram(program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    sh_loc_position = glGetAttribLocation(program, "position");
    sh_loc_texcoord = glGetAttribLocation(program, "texcoord");
    sh_loc_texture = glGetAttribLocation(program, "texture");
    glEnableVertexAttribArray(sh_loc_position);
    glEnableVertexAttribArray(sh_loc_texcoord);

    texture = create_texture();

    glGenBuffers(1, &vertex_buf);
    glGenBuffers(1, &texcoord_buf);
    glGenBuffers(1, &index_buf);

    return 0;
}

static void gl_deinit(void)
{
    glDeleteProgram(program);
    glDeleteTextures(1, &texture);
    glDeleteBuffers(1, &vertex_buf);
    glDeleteBuffers(1, &texcoord_buf);
    glDeleteBuffers(1, &index_buf);
}

static void draw(EGLDisplay eglDisplay, EGLSurface eglSurface)
{
    GLfloat vertex[] = {
	-1, 1, 0,
	-1, -1, 0,
	1, -1, 0,
	1, 1, 0,
    };
    GLfloat texcoord[] = {
	0, 0,
	0, 1,
	1, 1,
	1, 0,
    };
    GLushort index[] = {
	0, 1, 2, 0, 2, 3
    };

    glClearColor(1.0f, 1.f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(sh_loc_texture, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, vertex, GL_STATIC_DRAW);
    glVertexAttribPointer(sh_loc_position, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, texcoord_buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, texcoord,
		 GL_STATIC_DRAW);
    glVertexAttribPointer(sh_loc_texcoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLshort) * 6, index,
		 GL_STATIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    eglSwapBuffers(eglDisplay, eglSurface);
}

int main(void)
{
    struct sigaction sigint;
    Display *display;
    EGLNativeWindowType window;
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;

    xwindow_init(&display, &window);
    eglDisplay = eglGetDisplay(display);

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    egl_init(window, eglDisplay, &eglContext, &eglSurface);
    if (gl_init() < 0)
	return -1;

    running = 1;
    while (running) {
	draw(eglDisplay, eglSurface);
    }

    gl_deinit();
    egl_deinit(eglDisplay, eglSurface);

    return 0;
}
