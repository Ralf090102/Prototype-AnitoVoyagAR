#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <memory>
#include <chrono>

// AR plane representation
struct ARPlane {
    uint64_t id;                      // Unique identifier
    glm::mat4 pose;                   // Pose matrix of the plane
    std::vector<glm::vec3> vertices;  // Vertices defining the plane's shape
    glm::vec2 extent;                 // Width and height of the plane
    int32_t type;                     // Type of plane (floor, wall, etc)
    bool is_tracking;                 // Whether the plane is currently tracked
};

// AR anchor representation
struct ARAnchor {
    uint64_t id;                      // Unique identifier
    glm::mat4 pose;                   // Pose matrix of the anchor
    bool is_tracking;                 // Whether the anchor is currently tracked
};

// AR point cloud representation
struct ARPointCloud {
    std::vector<glm::vec3> points;    // 3D positions
    std::vector<float> confidences;   // Confidence values
    std::vector<uint64_t> ids;        // Point identifiers (optional)
    bool is_updated;                  // Whether new points were added
};

class ARTrackingManager {
public:
    ARTrackingManager();
    ~ARTrackingManager();

    // Camera tracking
    void UpdateCameraPose(const float* pose_matrix);
    const glm::mat4& GetCameraPose() const { return camera_pose_; }
    const glm::mat4& GetViewMatrix() const { return view_matrix_; }

    // Light estimation
    void UpdateLightEstimate(float ambient_intensity, float ambient_color[4]);
    float GetAmbientLightIntensity() const { return ambient_intensity_; }
    const glm::vec3& GetAmbientLightColor() const { return ambient_color_; }

    // Plane tracking
    void UpdatePlanes(const void* plane_data, int plane_count);
    const std::vector<ARPlane>& GetPlanes() const { return planes_; }
    ARPlane* GetPlaneById(uint64_t id);

    // Anchor management
    uint64_t CreateAnchor(const float* pose_matrix);
    void UpdateAnchor(uint64_t id, const float* pose_matrix, bool is_tracking);
    void RemoveAnchor(uint64_t id);
    const std::vector<ARAnchor>& GetAnchors() const { return anchors_; }
    ARAnchor* GetAnchorById(uint64_t id);

    // Point cloud
    void UpdatePointCloud(const float* points, const float* confidences,
                          int point_count, bool append = false);
    const ARPointCloud& GetPointCloud() const { return point_cloud_; }

    // Hit testing
    bool HitTest(float x, float y, glm::vec3* hit_position, glm::vec3* hit_normal);

    // Tracking state
    bool IsTracking() const { return is_tracking_; }
    void SetTrackingState(bool tracking) { is_tracking_ = tracking; }

    // Timing
    void UpdateFrameTime();
    float GetDeltaTime() const { return delta_time_; }

private:
    // Camera tracking
    glm::mat4 camera_pose_ = glm::mat4(1.0f);
    glm::mat4 view_matrix_ = glm::mat4(1.0f);

    // Light estimation
    float ambient_intensity_ = 1.0f;
    glm::vec3 ambient_color_ = glm::vec3(1.0f);

    // Tracked elements
    std::vector<ARPlane> planes_;
    std::vector<ARAnchor> anchors_;
    ARPointCloud point_cloud_;

    // Tracking state
    bool is_tracking_ = false;

    // Timing
    std::chrono::time_point<std::chrono::high_resolution_clock> last_frame_time_;
    float delta_time_ = 0.0f;

    // Helper methods
    void UpdateViewMatrix();
};
