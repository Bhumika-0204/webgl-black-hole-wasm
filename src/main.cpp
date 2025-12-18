#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cmath>

GLuint program, vao;
GLint uResLoc, uTimeLoc, uCamRotLoc;

float t = 0.0f;
int W = 0, H = 0;

/* ================= MOUSE DRIFT ================= */

float mouseNorm = 0.0f;     // -0.5 .. +0.5
float camTarget = 0.0f;
float camCurrent = 0.0f;

EM_BOOL mouse_cb(int, const EmscriptenMouseEvent* e, void*) {
    if (W > 0)
        mouseNorm = ((float)e->canvasX / (float)W) - 0.5f;
    return EM_TRUE;
}

/* ================= SHADERS ================= */

const char* vsSrc = R"(#version 300 es
precision highp float;
layout(location=0) in vec2 aPos;
void main(){
    gl_Position = vec4(aPos,0.0,1.0);
}
)";

const char* fsSrc = R"(#version 300 es
precision highp float;
out vec4 fragColor;

uniform vec2 uRes;
uniform float uTime;
uniform float uCamRot;

/* ---------- HELPERS ---------- */
mat2 rot(float a){
    float s = sin(a), c = cos(a);
    return mat2(c,-s,s,c);
}

float hash(vec2 p){
    return fract(sin(dot(p,vec2(127.1,311.7))) * 43758.545);
}

/* ---------- STARS ---------- */
float stars(vec2 uv){
    vec2 gv = fract(uv*90.0) - 0.5;
    vec2 id = floor(uv*90.0);
    float h = hash(id);
    float d = length(gv);
    return smoothstep(0.03, 0.0, d) * step(0.996, h);
}

void main(){
    /* ---------- NORMALIZED SPACE ---------- */
    vec2 uv = (gl_FragCoord.xy / uRes) * 2.0 - 1.0;
    uv.x *= uRes.x / uRes.y;

    /* ---------- CAMERA DRIFT ROTATION ---------- */
    uv *= rot(uCamRot);

    float r = length(uv);

    /* ---------- EVENT HORIZON ---------- */
    float horizon = smoothstep(0.18, 0.16, r);

    /* ---------- BACKGROUND LENSING ---------- */
    float lens = 1.0 / (1.0 + r * 1.4);
    vec2 bgUV = uv * lens;

    vec3 bg = vec3(stars(bgUV));

    /* ---------- ACCRETION DISK ---------- */
    vec2 diskUV = uv;
    diskUV *= rot(0.6);            // tilt
    diskUV *= rot(uTime * 0.4);    // auto swirl

    float dr = length(diskUV);

    float disk = exp(-abs(dr - 0.38) * 22.0);
    float thickness = exp(-abs(diskUV.y) * 8.0);
    disk *= thickness;

    vec3 diskCol = vec3(1.2, 0.55, 0.25) * disk;

    /* ---------- BLOOM ---------- */
    vec3 bloom = diskCol * diskCol * 1.3;

    /* ---------- COMPOSE ---------- */
    vec3 color = bg + diskCol + bloom;
    color *= (1.0 - horizon);

    fragColor = vec4(color, 1.0);
}
)";

/* ================= GL ================= */

GLuint compile(GLenum t, const char* s){
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    return sh;
}

void init(){
    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    float quad[] = { -1,-1,  1,-1,  -1,1,  1,1 };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    uResLoc    = glGetUniformLocation(program, "uRes");
    uTimeLoc   = glGetUniformLocation(program, "uTime");
    uCamRotLoc = glGetUniformLocation(program, "uCamRot");
}

void resize(){
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    W = (int)w;
    H = (int)h;
    emscripten_set_canvas_element_size("#canvas", W, H);
}

void frame(){
    t += 0.016f;
    resize();

    /* ---------- CAMERA DRIFT INERTIA ---------- */
    camTarget = mouseNorm * 1.2f;                     // sensitivity
    camCurrent += (camTarget - camCurrent) * 0.05f;   // damping

    glViewport(0, 0, W, H);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    glUniform2f(uResLoc, W, H);
    glUniform1f(uTimeLoc, t);
    glUniform1f(uCamRotLoc, camCurrent);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int main(){
    EmscriptenWebGLContextAttributes a;
    emscripten_webgl_init_context_attributes(&a);
    a.majorVersion = 2;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
        emscripten_webgl_create_context("#canvas", &a);
    emscripten_webgl_make_context_current(ctx);

    emscripten_set_mousemove_callback("#canvas", 0, true, mouse_cb);

    init();
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
