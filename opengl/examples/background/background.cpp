//OUT#include <iostream>
#include <cmath>

#include <emscripten.h>

// GLEW
#define GLEW_STATIC
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>

#define ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

// Function prototypes
static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}


// Window dimensions
const GLuint WIDTH = 800, HEIGHT = 600;

const GLchar* vertexShaderSource = R"(#version 330

uniform float time;
uniform vec2 resolution;
uniform float vertexCount;

out vec4 v_color;

#define PI radians(180.)

vec3 hsv2rgb(vec3 c) {
  c = vec3(c.x, clamp(c.yz, 0.0, 1.0));
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

mat4 rotX(float angleInRadians) {
    float s = sin(angleInRadians);
    float c = cos(angleInRadians);

    return mat4(
      1, 0, 0, 0,
      0, c, s, 0,
      0, -s, c, 0,
      0, 0, 0, 1);
}

mat4 rotY(float angleInRadians) {
    float s = sin(angleInRadians);
    float c = cos(angleInRadians);

    return mat4(
      c, 0,-s, 0,
      0, 1, 0, 0,
      s, 0, c, 0,
      0, 0, 0, 1);
}

mat4 rotZ(float angleInRadians) {
    float s = sin(angleInRadians);
    float c = cos(angleInRadians);

    return mat4(
      c,-s, 0, 0,
      s, c, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1);
}

mat4 trans(vec3 trans) {
  return mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    trans, 1);
}

mat4 ident() {
  return mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1);
}

mat4 scale(vec3 s) {
  return mat4(
    s[0], 0, 0, 0,
    0, s[1], 0, 0,
    0, 0, s[2], 0,
    0, 0, 0, 1);
}

mat4 uniformScale(float s) {
  return mat4(
    s, 0, 0, 0,
    0, s, 0, 0,
    0, 0, s, 0,
    0, 0, 0, 1);
}

mat4 persp(float fov, float aspect, float zNear, float zFar) {
  float f = tan(PI * 0.5 - 0.5 * fov);
  float rangeInv = 1.0 / (zNear - zFar);

  return mat4(
    f / aspect, 0, 0, 0,
    0, f, 0, 0,
    0, 0, (zNear + zFar) * rangeInv, -1,
    0, 0, zNear * zFar * rangeInv * 2., 0);
}

mat4 trInv(mat4 m) {
  mat3 i = mat3(
    m[0][0], m[1][0], m[2][0],
    m[0][1], m[1][1], m[2][1],
    m[0][2], m[1][2], m[2][2]);
  vec3 t = -i * m[3].xyz;

  return mat4(
    i[0], t[0],
    i[1], t[1],
    i[2], t[2],
    0, 0, 0, 1);
}

mat4 lookAt(vec3 eye, vec3 target, vec3 up) {
  vec3 zAxis = normalize(eye - target);
  vec3 xAxis = normalize(cross(up, zAxis));
  vec3 yAxis = cross(zAxis, xAxis);

  return mat4(
    xAxis, 0,
    yAxis, 0,
    zAxis, 0,
    eye, 1);
}

mat4 cameraLookAt(vec3 eye, vec3 target, vec3 up) {
  return inverse(lookAt(eye, target, up));
}

// hash function from https://www.shadertoy.com/view/4djSRW
float hash(float p) {
    vec2 p2 = fract(vec2(p * 5.3983, p * 5.4427));
    p2 += dot(p2.yx, p2.xy + vec2(21.5351, 14.3137));
    return fract(p2.x * p2.y * 95.4337);
}

// times 2 minus 1
float t2m1(float v) {
  return v * 2. - 1.;
}

// times .5 plus .5
float t5p5(float v) {
  return v * 0.5 + 0.5;
}

float inv(float v) {
  return 1. - v;
}

#define CUBE_POINTS_PER_FACE 6.
#define FACES_PER_CUBE 6.
#define POINTS_PER_CUBE (CUBE_POINTS_PER_FACE * FACES_PER_CUBE)
void getCubePoint(const float id, out vec3 position, out vec3 normal) {
  float quadId = floor(mod(id, POINTS_PER_CUBE) / CUBE_POINTS_PER_FACE);
  float sideId = mod(quadId, 3.);
  float flip   = mix(1., -1., step(2.5, quadId));
  // 0 1 2  1 2 3
  float facePointId = mod(id, CUBE_POINTS_PER_FACE);
  float pointId = mod(facePointId - floor(facePointId / 3.0), 6.0);
  float a = pointId * PI * 2. / 4. + PI * 0.25;
  vec3 p = vec3(cos(a), 0.707106781, sin(a)) * flip;
  vec3 n = vec3(0, 1, 0) * flip;
  float lr = mod(sideId, 2.);
  float ud = step(2., sideId);
  mat4 mat = rotX(lr * PI * 0.5);
  mat *= rotZ(ud * PI * 0.5);
  position = (mat * vec4(p, 1)).xyz;
  normal = (mat * vec4(n, 0)).xyz;
}

void main() {
  float pointId = float(gl_VertexID);

  vec3 pos;
  vec3 normal;
  getCubePoint(pointId, pos, normal);
  float cubeId = floor(pointId / 36.);
  float numCubes = floor(vertexCount / 36.);
  float down = floor(sqrt(numCubes));
  float across = floor(numCubes / down);

  float cx = mod(cubeId, across);
  float cy = floor(cubeId / across);

  float cu = cx / (across - 1.);
  float cv = cy / (down - 1.);

  float ca = cu * 2. - 1.;
  float cd = cv * 2. - 1.;

  float tm = time * 0.1;
  mat4 mat = persp(radians(60.0), resolution.x / resolution.y, 0.1, 1000.0);
  vec3 eye = vec3(cos(tm) * 1., sin(tm * 0.9) * .1 + 1., sin(tm) * 1.);
  vec3 target = vec3(0);
  vec3 up = vec3(0,1,0);

  mat *= cameraLookAt(eye, target, up);
  mat *= trans(vec3(ca, 0, cd) * 2.);
  mat *= rotX(time + abs(ca) * 5.);
  mat *= rotZ(time + abs(cd) * 6.);
  mat *= uniformScale(0.03);


  gl_Position = mat * vec4(pos, 1);
  vec3 n = normalize((mat * vec4(normal, 0)).xyz);

  vec3 lightDir = normalize(vec3(0.3, 0.4, -1));

  float hue = abs(ca * cd) * 2.;
  float sat = mix(1., 0., abs(ca));
  float val = mix(1., 0.5, abs(cd));
  vec3 color = hsv2rgb(vec3(hue, sat, val));
  v_color = vec4(color * (dot(n, lightDir) * 0.5 + 0.5), 1);
}
)";

const GLchar* fragmentShaderSource = R"(#version 330
in vec4 v_color;
out vec4 color;
void main()
{
   color = v_color;
}
)";

    static GLuint VBO, VAO, EBO;
    static GLuint shaderProgram;
    static GLFWwindow* window;
    static GLint resolutionLoc;
    static GLint timeLoc;
    static GLint vertexCountLoc;

void cleanup() {
    // Properly de-allocate all resources once they've outlived their purpose
    glDeleteVertexArrays(1, &VAO);
//    glDeleteBuffers(1, &VBO);
//    glDeleteBuffers(1, &EBO);
    // Terminate GLFW, clearing any resources allocated by GLFW.
    glfwTerminate();
    //OUT std::cout << "exit main" << std::endl;
}

void Refresh(GLFWwindow* window)
{
    glfwMarkWindowForRefresh(window);
}

void mainLoop(GLFWwindow* window) {
    // Game loop
    if (glfwWindowShouldClose(window)) {
      cleanup();
      return;
    }

    // Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
    glfwPollEvents();

    // Rendering
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    static float time = 0.f;
    time += 1.0f/60.0f;

    glEnable(GL_DEPTH_TEST);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glUniform1f(timeLoc, time);
    int numVerts = 100000;
    glUniform1f(vertexCountLoc, numVerts);
    glUniform2f(resolutionLoc, display_w, display_h);
    glDrawArrays(GL_TRIANGLES, 0, numVerts);
    glBindVertexArray(0);

    glfwSwapBuffers(window);

    Refresh(window);
}

void FramebufferSizeCallback(GLFWwindow* window, int /* width */, int /* height */)
{
    Refresh(window);
}

void WindowFocusCallback(GLFWwindow* window, int focused)
{
    if (focused)
    {
        Refresh(window);
    }
}

// The MAIN function, from here we start the application and run the game loop
int main()
{
    glfwSetErrorCallback(error_callback);

    // Init GLFW
    glfwInit();
    // Set all the required options for GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);

    // Create a GLFWwindow object that we can use for GLFW's functions
    window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Set this to true so GLEW knows to use a modern approach to retrieving function pointers and extensions
    glewExperimental = GL_TRUE;
    // Initialize GLEW to setup the OpenGL Function pointers
    glewInit();

    glfwSetWindowFocusCallback(window, WindowFocusCallback);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // Define the viewport dimensions
    static int width, height;
    glfwGetFramebufferSize(window, &width, &height);  
    glViewport(0, 0, width, height);


    // Build and compile our shader program
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // Check for compile time errors
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED: %s\n", infoLog);
        return 0;
    }
    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // Check for compile time errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED: %s\n", infoLog);
        return 0;
    }
    // Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("ERROR::PROGRAM::LINK_FAILED: %s\n", infoLog);
        return 0;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    resolutionLoc = glGetUniformLocation(shaderProgram, "resolution");
    timeLoc = glGetUniformLocation(shaderProgram, "time");
    vertexCountLoc = glGetUniformLocation(shaderProgram, "vertexCount");

//    // Set up vertex data (and buffer(s)) and attribute pointers
//    //GLfloat vertices[] = {
//    //  // First triangle
//    //   0.5f,  0.5f,  // Top Right
//    //   0.5f, -0.5f,  // Bottom Right
//    //  -0.5f,  0.5f,  // Top Left
//    //  // Second triangle
//    //   0.5f, -0.5f,  // Bottom Right
//    //  -0.5f, -0.5f,  // Bottom Left
//    //  -0.5f,  0.5f   // Top Left
//    //};
//    static GLfloat vertices[] = {
//         0.5f,  0.5f, 0.0f,  // Top Right
//         0.5f, -0.5f, 0.0f,  // Bottom Right
//        -0.5f, -0.5f, 0.0f,  // Bottom Left
//        -0.5f,  0.5f, 0.0f   // Top Left
//    };
//    static GLuint indices[] = {  // Note that we start from 0!
//        0, 1, 3,  // First Triangle
//        1, 2, 3   // Second Triangle
//    };
    glGenVertexArrays(1, &VAO);
//    glGenBuffers(1, &VBO);
//    glGenBuffers(1, &EBO);
    // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
    glBindVertexArray(VAO);

//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
//
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
//
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
//    glEnableVertexAttribArray(0);
//
//    glBindBuffer(GL_ARRAY_BUFFER, 0); // Note that this is allowed, the call to glVertexAttribPointer registered VBO as the currently bound vertex buffer object so afterwards we can safely unbind

    glBindVertexArray(0); // Unbind VAO (it's always a good thing to unbind any buffer/array to prevent strange bugs), remember: do NOT unbind the EBO, keep it bound to this VAO

    glfwSetWindowRefreshCallback(window, mainLoop);
    Refresh(window);
    return 0;
}

