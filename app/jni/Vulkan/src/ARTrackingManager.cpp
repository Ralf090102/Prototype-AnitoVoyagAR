#include "ARTrackingManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <chrono>

ARTrackingManager::ARTrackingManager() {
    // Initialize timing
    last_frame_time_ = std::chrono::high_resolution_clock::now();
}

ARTrackingManager::~ARTrackingManager() {
    // Clean up resources if needed
}

void ARTrackingManager::UpdateCameraPose(const float* pose_matrix) {
    // Update the camera pose matrix (column-major to match OpenGL/OpenXR convention)
    camera_pose_ = glm::make_mat4(pose_matrix);

    // Update the view matrix
    UpdateViewMatrix();
}

void ARTrackingManager::UpdateViewMatrix() {
    // The view matrix is the inverse of the camera pose matrix
    view_matrix_ = glm::inverse(camera_pose_);
}

void ARTrackingManager::UpdateLightEstimate(float ambient_intensity, float ambient_color[4]) {
    ambient_intensity_ = ambient_intensity;
    ambient_color_ = glm::vec3(ambient_color[0], ambient_color[1], ambient_color[2]);
}

void ARTrackingManager::UpdatePlanes(const void* plane_data, int plane_count) {
    // Clear old plane data but preserve IDs that are still valid
    std::vector<ARPlane> updated_planes;
    updated_planes.reserve(plane_count);

    // Process plane data from ARCore
    // This is a simplified placeholder - actual implementation will depend on ARCore's data format

    // For each plane from ARCore
    const float* plane_ptr = static_cast<const float*>(plane_data);
    for (int i = 0; i < plane_count; i++) {
        ARPlane plane;

        // Extract plane ID (example)
        uint64_t plane_id = static_cast<uint64_t>(plane_ptr[0]);

        // Find if this plane was already tracked
        auto existing_plane = std::find_if(planes_.begin(), planes_.end(),
                                           [plane_id](const ARPlane& p) { return p.id == plane_id; });

        if (existing_plane != planes_.end()) {
            // Update existing plane
            plane = *existing_plane;
        } else {
            // Create new plane
            plane.id = plane_id;
        }

        // Update plane data (example)
        // This would be replaced with actual ARCore plane data extraction
        plane_ptr += sizeof(uint64_t) / sizeof(float); // Skip ID

        // Example: Extract pose matrix
        plane.pose = glm::make_mat4(plane_ptr);
        plane_ptr += 16; // Skip pose matrix

        // Example: Extract extent
        plane.extent.x = *plane_ptr++;
        plane.extent.y = *plane_ptr++;

        // Example: Extract type
        plane.type = static_cast<int32_t>(*plane_ptr++);

        // Example: Extract vertices
        int vertex_count = static_cast<int>(*plane_ptr++);
        plane.vertices.resize(vertex_count);
        for (int j = 0; j < vertex_count; j++) {
            plane.vertices[j].x = *plane_ptr++;
            plane.vertices[j].y = *plane_ptr++;
            plane.vertices[j].z = *plane_ptr++;
        }

        plane.is_tracking = true;
        updated_planes.push_back(plane);
    }

    // Mark planes not found in the update as not tracking
    for (const auto& old_plane : planes_) {
        auto it = std::find_if(updated_planes.begin(), updated_planes.end(),
                               [&old_plane](const ARPlane& p) { return p.id == old_plane.id; });

        if (it == updated_planes.end()) {
            // Plane no longer tracked - add it to the list but mark as not tracking
            ARPlane lost_plane = old_plane;
            lost_plane.is_tracking = false;
            updated_planes.push_back(lost_plane);
        }
    }

    // Replace plane list with updated list
    planes_ = std::move(updated_planes);
}

ARPlane* ARTrackingManager::GetPlaneById(uint64_t id) {
    auto it = std::find_if(planes_.begin(), planes_.end(),
                           [id](const ARPlane& p) { return p.id == id; });

    if (it != planes_.end()) {
        return &(*it);
    }

    return nullptr;
}

uint64_t ARTrackingManager::CreateAnchor(const float* pose_matrix) {
    static uint64_t next_anchor_id = 1;

    ARAnchor anchor;
    anchor.id = next_anchor_id++;
    anchor.pose = glm::make_mat4(pose_matrix);
    anchor.is_tracking = true;

    anchors_.push_back(anchor);
    return anchor.id;
}

void ARTrackingManager::UpdateAnchor(uint64_t id, const float* pose_matrix, bool is_tracking) {
    auto it = std::find_if(anchors_.begin(), anchors_.end(),
                           [id](const ARAnchor& a) { return a.id == id; });

    if (it != anchors_.end()) {
        it->pose = glm::make_mat4(pose_matrix);
        it->is_tracking = is_tracking;
    }
}

void ARTrackingManager::RemoveAnchor(uint64_t id) {
    auto it = std::find_if(anchors_.begin(), anchors_.end(),
                           [id](const ARAnchor& a) { return a.id == id; });

    if (it != anchors_.end()) {
        anchors_.erase(it);
    }
}

ARAnchor* ARTrackingManager::GetAnchorById(uint64_t id) {
    auto it = std::find_if(anchors_.begin(), anchors_.end(),
                           [id](const ARAnchor& a) { return a.id == id; });

    if (it != anchors_.end()) {
        return &(*it);
    }

    return nullptr;
}

void ARTrackingManager::UpdatePointCloud(const float* points, const float* confidences,
                                         int point_count, bool append) {
    if (!append) {
        point_cloud_.points.clear();
        point_cloud_.confidences.clear();
        point_cloud_.ids.clear();
    }

    size_t start_index = point_cloud_.points.size();
    point_cloud_.points.resize(start_index + point_count);
    point_cloud_.confidences.resize(start_index + point_count);

    // Copy points and confidences
    for (int i = 0; i < point_count; i++) {
        point_cloud_.points[start_index + i] = glm::vec3(
                points[i * 3],
                points[i * 3 + 1],
                points[i * 3 + 2]
        );

        point_cloud_.confidences[start_index + i] = confidences[i];
    }

    point_cloud_.is_updated = true;
}

bool ARTrackingManager::HitTest(float x, float y, glm::vec3* hit_position, glm::vec3* hit_normal) {
    // This is a placeholder implementation
    // In a real app, you would use ARCore's hit testing functionality

    // Check if we have any planes to hit test against
    if (planes_.empty()) {
        return false;
    }

    // For simplicity, let's assume x and y are normalized screen coordinates [-1, 1]
    // In reality, you'd use ARCore's hit testing with your actual viewport coordinates

    // Find a plane to hit against (just pick the first tracking plane for this example)
    const ARPlane *target_plane = nullptr;
    for (const auto &plane: planes_) {
        if (plane.is_tracking) {
            target_plane = &plane;
            break;
        }
    }

    if (!target_plane) {
        return false;
    }

    // Get plane center in world space
    glm::vec3 plane_center = glm::vec3(target_plane->pose[3]);

    // Plane normal is typically the y-axis of the plane's coordinate system
    glm::vec3 plane_normal = glm::normalize(glm::vec3(target_plane->pose[1]));

    // Create a ray from the camera position through the tap point
    glm::vec3 camera_pos = glm::vec3(camera_pose_[3]);

    // Create a vector pointing from camera in the tap direction
    // In a real implementation, you'd use proper unprojection based on your perspective matrix
    glm::vec3 ray_dir = glm::normalize(glm::vec3(x, y, -1.0f));
    ray_dir = glm::mat3(camera_pose_) * ray_dir; // Transform to world space

    // Ray-plane intersection
    float denominator = glm::dot(ray_dir, plane_normal);
    if (std::abs(denominator) < 0.0001f) {
        return false; // Ray is parallel to plane
    }

    float t = glm::dot(plane_center - camera_pos, plane_normal) / denominator;
    if (t < 0) {
        return false; // Intersection is behind camera
    }

    // Compute intersection point
    glm::vec3 intersection = camera_pos + ray_dir * t;

    // Check if intersection is within plane bounds (simplified)
    // Transform intersection to plane's local space
    glm::mat4 plane_transform_inv = glm::inverse(target_plane->pose);
    glm::vec3 local_intersection = glm::vec3(plane_transform_inv * glm::vec4(intersection, 1.0f));

    // Check if point is within extent
    if (std::abs(local_intersection.x) > target_plane->extent.x / 2.0f ||
        std::abs(local_intersection.z) > target_plane->extent.y / 2.0f) {
        return false;
    }

    // We have a valid hit, fill in the output parameters
    if (hit_position) {
        *hit_position = intersection;
    }

    if (hit_normal) {
        *hit_normal = plane_normal;
    }

    return true;
}


void ARTrackingManager::UpdateFrameTime() {
    auto current_time = std::chrono::high_resolution_clock::now();
    delta_time_ = std::chrono::duration<float, std::chrono::seconds::period>(
            current_time - last_frame_time_).count();
    last_frame_time_ = current_time;
}