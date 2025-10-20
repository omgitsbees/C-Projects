#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>
 
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

// Forward declarations
class Shader;
class Texture;
class Material;
class Mesh;
class Model;
class Light;
class Camera;
class SceneNode;
class Renderer;

// ===== SHADER CLASS =====
class Shader {
private:
    GLuint programID;
    std::unordered_map<std::string, GLint> uniformCache;

    GLint getUniformLocation(const std::string& name) {
        if (uniformCache.find(name) != uniformCache.end())
            return uniformCache[name];
        
        GLint location = glGetUniformLocation(programID, name.c_str());
        uniformCache[name] = location;
        return location;
    }

    std::string loadShaderSource(const std::string& filepath) {
        std::ifstream file(filepath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint compileShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cout << "Shader compilation failed: " << infoLog << std::endl;
        }
        return shader;
    }

public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath) {
        std::string vertexSource = loadShaderSource(vertexPath);
        std::string fragmentSource = loadShaderSource(fragmentPath);

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        programID = glCreateProgram();
        glAttachShader(programID, vertexShader);
        glAttachShader(programID, fragmentShader);
        glLinkProgram(programID);

        GLint success;
        glGetProgramiv(programID, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(programID, 512, nullptr, infoLog);
            std::cout << "Shader linking failed: " << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    ~Shader() {
        glDeleteProgram(programID);
    }

    void use() const {
        glUseProgram(programID);
    }

    void setMat4(const std::string& name, const glm::mat4& mat) {
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void setVec3(const std::string& name, const glm::vec3& vec) {
        glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(vec));
    }

    void setFloat(const std::string& name, float value) {
        glUniform1f(getUniformLocation(name), value);
    }

    void setInt(const std::string& name, int value) {
        glUniform1i(getUniformLocation(name), value);
    }
};

// ===== TEXTURE CLASS =====
class Texture {
private:
    GLuint textureID;
    GLenum type;

public:
    Texture(const std::string& path, GLenum textureType = GL_TEXTURE_2D) 
        : type(textureType) {
        glGenTextures(1, &textureID);
        glBindTexture(type, textureID);

        int width, height, nrChannels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
        
        if (data) {
            GLenum format = GL_RGB;
            if (nrChannels == 4) format = GL_RGBA;
            else if (nrChannels == 1) format = GL_RED;

            glTexImage2D(type, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(type);

            glTexParameteri(type, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(type, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            std::cout << "Failed to load texture: " << path << std::endl;
        }
        
        stbi_image_free(data);
    }

    ~Texture() {
        glDeleteTextures(1, &textureID);
    }

    void bind(GLuint unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(type, textureID);
    }

    GLuint getID() const { return textureID; }
};

// ===== MATERIAL CLASS =====
class Material {
public:
    glm::vec3 ambient{0.1f};
    glm::vec3 diffuse{0.8f};
    glm::vec3 specular{1.0f};
    float shininess = 32.0f;
    
    std::shared_ptr<Texture> diffuseMap;
    std::shared_ptr<Texture> specularMap;
    std::shared_ptr<Texture> normalMap;

    void applyToShader(Shader& shader) const {
        shader.setVec3("material.ambient", ambient);
        shader.setVec3("material.diffuse", diffuse);
        shader.setVec3("material.specular", specular);
        shader.setFloat("material.shininess", shininess);

        if (diffuseMap) {
            diffuseMap->bind(0);
            shader.setInt("material.diffuseMap", 0);
        }
        if (specularMap) {
            specularMap->bind(1);
            shader.setInt("material.specularMap", 1);
        }
        if (normalMap) {
            normalMap->bind(2);
            shader.setInt("material.normalMap", 2);
        }
    }
};

// ===== VERTEX STRUCTURE =====
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

// ===== MESH CLASS =====
class Mesh {
private:
    GLuint VAO, VBO, EBO;

public:
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    Material material;

    Mesh(const std::vector<Vertex>& verts, const std::vector<GLuint>& inds)
        : vertices(verts), indices(inds) {
        setupMesh();
    }

    ~Mesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), 
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), 
                     indices.data(), GL_STATIC_DRAW);

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        // Normal attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                              (void*)offsetof(Vertex, normal));

        // Texture coordinate attribute
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                              (void*)offsetof(Vertex, texCoords));

        // Tangent attribute
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                              (void*)offsetof(Vertex, tangent));

        // Bitangent attribute
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                              (void*)offsetof(Vertex, bitangent));

        glBindVertexArray(0);
    }

    void draw(Shader& shader) const {
        material.applyToShader(shader);
        
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// ===== MODEL CLASS =====
class Model {
private:
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::string directory;

    void loadModel(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, 
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
            return;
        }

        directory = path.substr(0, path.find_last_of('/'));
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    std::unique_ptr<Mesh> processMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;

        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            
            vertex.position = glm::vec3(mesh->mVertices[i].x, 
                                      mesh->mVertices[i].y, 
                                      mesh->mVertices[i].z);

            if (mesh->HasNormals()) {
                vertex.normal = glm::vec3(mesh->mNormals[i].x, 
                                        mesh->mNormals[i].y, 
                                        mesh->mNormals[i].z);
            }

            if (mesh->mTextureCoords[0]) {
                vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, 
                                           mesh->mTextureCoords[0][i].y);

                vertex.tangent = glm::vec3(mesh->mTangents[i].x, 
                                         mesh->mTangents[i].y, 
                                         mesh->mTangents[i].z);
                vertex.bitangent = glm::vec3(mesh->mBitangents[i].x, 
                                           mesh->mBitangents[i].y, 
                                           mesh->mBitangents[i].z);
            } else {
                vertex.texCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        return std::make_unique<Mesh>(vertices, indices);
    }

public:
    Model(const std::string& path) {
        loadModel(path);
    }

    void draw(Shader& shader) const {
        for (const auto& mesh : meshes) {
            mesh->draw(shader);
        }
    }
};

// ===== LIGHT CLASSES =====
struct DirectionalLight {
    glm::vec3 direction{-0.2f, -1.0f, -0.3f};
    glm::vec3 ambient{0.05f};
    glm::vec3 diffuse{0.4f};
    glm::vec3 specular{0.5f};

    void applyToShader(Shader& shader) const {
        shader.setVec3("dirLight.direction", direction);
        shader.setVec3("dirLight.ambient", ambient);
        shader.setVec3("dirLight.diffuse", diffuse);
        shader.setVec3("dirLight.specular", specular);
    }
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 ambient{0.05f};
    glm::vec3 diffuse{0.8f};
    glm::vec3 specular{1.0f};
    
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    void applyToShader(Shader& shader, int index) const {
        std::string base = "pointLights[" + std::to_string(index) + "]";
        shader.setVec3(base + ".position", position);
        shader.setVec3(base + ".ambient", ambient);
        shader.setVec3(base + ".diffuse", diffuse);
        shader.setVec3(base + ".specular", specular);
        shader.setFloat(base + ".constant", constant);
        shader.setFloat(base + ".linear", linear);
        shader.setFloat(base + ".quadratic", quadratic);
    }
};

struct SpotLight {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    float cutOff = glm::cos(glm::radians(12.5f));
    float outerCutOff = glm::cos(glm::radians(15.0f));
    
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{1.0f};
    glm::vec3 specular{1.0f};
    
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    void applyToShader(Shader& shader) const {
        shader.setVec3("spotLight.position", position);
        shader.setVec3("spotLight.direction", direction);
        shader.setFloat("spotLight.cutOff", cutOff);
        shader.setFloat("spotLight.outerCutOff", outerCutOff);
        shader.setVec3("spotLight.ambient", ambient);
        shader.setVec3("spotLight.diffuse", diffuse);
        shader.setVec3("spotLight.specular", specular);
        shader.setFloat("spotLight.constant", constant);
        shader.setFloat("spotLight.linear", linear);
        shader.setFloat("spotLight.quadratic", quadratic);
    }
};

// ===== CAMERA CLASS =====
class Camera {
public:
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right;
    glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
    
    float yaw = -90.0f;
    float pitch = 0.0f;
    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;
    float zoom = 45.0f;

    Camera() {
        updateCameraVectors();
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void processKeyboard(int direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        if (direction == 0) position += front * velocity;        // Forward
        if (direction == 1) position -= front * velocity;        // Backward  
        if (direction == 2) position -= right * velocity;        // Left
        if (direction == 3) position += right * velocity;        // Right
    }

    void processMouseMovement(float xoffset, float yoffset) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        updateCameraVectors();
    }

    void processMouseScroll(float yoffset) {
        zoom -= yoffset;
        if (zoom < 1.0f) zoom = 1.0f;
        if (zoom > 45.0f) zoom = 45.0f;
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        this->front = glm::normalize(front);
        right = glm::normalize(glm::cross(this->front, worldUp));
        up = glm::normalize(glm::cross(right, this->front));
    }
};

// ===== SCENE NODE CLASS =====
class SceneNode {
public:
    glm::mat4 transform{1.0f};
    std::vector<std::unique_ptr<SceneNode>> children;
    std::shared_ptr<Model> model;
    
    SceneNode() = default;
    
    void addChild(std::unique_ptr<SceneNode> child) {
        children.push_back(std::move(child));
    }
    
    void setModel(std::shared_ptr<Model> m) {
        model = m;
    }
    
    void draw(Shader& shader, const glm::mat4& parentTransform = glm::mat4(1.0f)) const {
        glm::mat4 currentTransform = parentTransform * transform;
        shader.setMat4("model", currentTransform);
        
        if (model) {
            model->draw(shader);
        }
        
        for (const auto& child : children) {
            child->draw(shader, currentTransform);
        }
    }
};

// ===== SHADOW MAPPING CLASS =====
class ShadowMapper {
private:
    GLuint depthMapFBO;
    GLuint depthMap;
    const GLuint SHADOW_WIDTH = 1024;
    const GLuint SHADOW_HEIGHT = 1024;

public:
    ShadowMapper() {
        // Create depth map texture
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
                     SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Create framebuffer
        glGenFramebuffers(1, &depthMapFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ~ShadowMapper() {
        glDeleteFramebuffers(1, &depthMapFBO);
        glDeleteTextures(1, &depthMap);
    }

    void beginShadowPass() {
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void endShadowPass(int screenWidth, int screenHeight) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
    }

    void bindShadowMap(GLuint textureUnit) {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, depthMap);
    }

    glm::mat4 getLightSpaceMatrix(const glm::vec3& lightDir) {
        float near_plane = 1.0f, far_plane = 7.5f;
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
        glm::mat4 lightView = glm::lookAt(-lightDir, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        return lightProjection * lightView;
    }
};

// ===== MAIN RENDERER CLASS =====
class Renderer {
private:
    std::unique_ptr<Shader> mainShader;
    std::unique_ptr<Shader> shadowShader;
    std::unique_ptr<ShadowMapper> shadowMapper;
    
public:
    DirectionalLight dirLight;
    std::vector<PointLight> pointLights;
    SpotLight spotLight;
    
    Renderer() {
        // Initialize shaders (you'll need to create the shader files)
        mainShader = std::make_unique<Shader>("shaders/main.vert", "shaders/main.frag");
        shadowShader = std::make_unique<Shader>("shaders/shadow.vert", "shaders/shadow.frag");
        shadowMapper = std::make_unique<ShadowMapper>();
    }
    
    void render(const SceneNode& scene, const Camera& camera, int screenWidth, int screenHeight) {
        // 1. Shadow pass
        glm::mat4 lightSpaceMatrix = shadowMapper->getLightSpaceMatrix(dirLight.direction);
        shadowMapper->beginShadowPass();
        shadowShader->use();
        shadowShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        scene.draw(*shadowShader);
        shadowMapper->endShadowPass(screenWidth, screenHeight);
        
        // 2. Main render pass
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        mainShader->use();
        
        // Set matrices
        glm::mat4 projection = glm::perspective(glm::radians(camera.zoom), 
                                              (float)screenWidth / screenHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.getViewMatrix();
        
        mainShader->setMat4("projection", projection);
        mainShader->setMat4("view", view);
        mainShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        mainShader->setVec3("viewPos", camera.position);
        
        // Set lights
        dirLight.applyToShader(*mainShader);
        for (size_t i = 0; i < pointLights.size() && i < 4; i++) {
            pointLights[i].applyToShader(*mainShader, i);
        }
        spotLight.applyToShader(*mainShader);
        
        // Bind shadow map
        shadowMapper->bindShadowMap(3);
        mainShader->setInt("shadowMap", 3);
        
        // Render scene
        scene.draw(*mainShader);
    }
};

// ===== GRAPHICS ENGINE CLASS =====
class GraphicsEngine {
private:
    GLFWwindow* window;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<SceneNode> rootNode;
    
    int screenWidth = 1200;
    int screenHeight = 800;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    // Mouse input
    float lastX = screenWidth / 2.0f;
    float lastY = screenHeight / 2.0f;
    bool firstMouse = true;

public:
    GraphicsEngine() {
        initOpenGL();
        renderer = std::make_unique<Renderer>();
        camera = std::make_unique<Camera>();
        rootNode = std::make_unique<SceneNode>();
        
        // Setup lighting
        setupLights();
    }
    
    ~GraphicsEngine() {
        glfwTerminate();
    }
    
    bool initOpenGL() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        window = glfwCreateWindow(screenWidth, screenHeight, "3D Graphics Engine", NULL, NULL);
        if (!window) {
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);
        
        // Set callbacks
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        
        if (glewInit() != GLEW_OK) {
            return false;
        }
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        
        return true;
    }
    
    void setupLights() {
        // Setup directional light
        renderer->dirLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        renderer->dirLight.ambient = glm::vec3(0.05f);
        renderer->dirLight.diffuse = glm::vec3(0.4f);
        renderer->dirLight.specular = glm::vec3(0.5f);
        
        // Add some point lights
        PointLight pointLight1;
        pointLight1.position = glm::vec3(0.7f, 0.2f, 2.0f);
        pointLight1.ambient = glm::vec3(0.05f);
        pointLight1.diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
        pointLight1.specular = glm::vec3(1.0f);
        renderer->pointLights.push_back(pointLight1);
        
        // Setup spot light (flashlight)
        renderer->spotLight.position = camera->position;
        renderer->spotLight.direction = camera->front;
    }
    
    void loadScene() {
        // Example: Load a model and add it to the scene
        // auto model = std::make_shared<Model>("models/backpack/backpack.obj");
        // rootNode->setModel(model);
        
        // You can create a hierarchy of nodes here
        auto childNode = std::make_unique<SceneNode>();
        childNode->transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
        // childNode->setModel(anotherModel);
        rootNode->addChild(std::move(childNode));
    }
    
    void run() {
        loadScene();
        
        while (!glfwWindowShouldClose(window)) {
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            
            processInput();
            
            // Update spot light to follow camera
            renderer->spotLight.position = camera->position;
            renderer->spotLight.direction = camera->front;
            
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            renderer->render(*rootNode, *camera, screenWidth, screenHeight);
            
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    
    void processInput() {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera->processKeyboard(0, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera->processKeyboard(1, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera->processKeyboard(2, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera->processKeyboard(3, deltaTime);
    }
    
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
        auto* engine = static_cast<GraphicsEngine*>(glfwGetWindowUserPointer(window));
        engine->screenWidth = width;
        engine->screenHeight = height;
        glViewport(0, 0, width, height);
    }
    
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* engine = static_cast<GraphicsEngine*>(glfwGetWindowUserPointer(window));
        
        if (engine->firstMouse) {
            engine->lastX = xpos;
            engine->lastY = ypos;
            engine->firstMouse = false;
        }
        
        float xoffset = xpos - engine->lastX;
        float yoffset = engine->lastY - ypos;
        
        engine->lastX = xpos;
        engine->lastY = ypos;
        
        engine->camera->processMouseMovement(xoffset, yoffset);
    }
    
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* engine = static_cast<GraphicsEngine*>(glfwGetWindowUserPointer(window));
        engine->camera->processMouseScroll(yoffset);
    }
};

// ===== MAIN FUNCTION =====
int main() {
    GraphicsEngine engine;
    engine.run();
    return 0;

}
