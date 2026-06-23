#include "AnimatedModel.h"

#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/type_ptr.hpp>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace
{
    constexpr int kVertexStride = 11;

    glm::vec3 makeVec3(const cgltf_float* values, const glm::vec3& fallback)
    {
        return values == nullptr ? fallback : glm::vec3(values[0], values[1], values[2]);
    }

    glm::quat makeQuat(const cgltf_float* values)
    {
        return values == nullptr
            ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
            : glm::normalize(glm::quat(values[3], values[0], values[1], values[2]));
    }

    glm::mat4 makeMat4(const cgltf_float* values)
    {
        glm::mat4 matrix(1.0f);
        if (values != nullptr)
        {
            std::memcpy(glm::value_ptr(matrix), values, sizeof(float) * 16);
        }
        return matrix;
    }

    glm::mat4 composeTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
    {
        return glm::translate(glm::mat4(1.0f), translation) *
            glm::mat4_cast(rotation) *
            glm::scale(glm::mat4(1.0f), scale);
    }

    void readEmbeddedImage(const cgltf_image* image, std::vector<unsigned char>& output)
    {
        if (image == nullptr || image->buffer_view == nullptr || image->buffer_view->size == 0)
        {
            return;
        }

        const uint8_t* bytes = cgltf_buffer_view_data(image->buffer_view);
        if (bytes == nullptr)
        {
            return;
        }

        output.assign(bytes, bytes + image->buffer_view->size);
    }
}

struct AnimatedModel::Impl
{
    struct Vertex
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec2 texCoord = glm::vec2(0.0f);
        glm::vec3 color = glm::vec3(1.0f);
        glm::uvec4 joints = glm::uvec4(0);
        glm::vec4 weights = glm::vec4(0.0f);
    };

    struct NodePose
    {
        int parent = -1;
        std::vector<int> children;
        glm::vec3 baseTranslation = glm::vec3(0.0f);
        glm::quat baseRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 baseScale = glm::vec3(1.0f);
        glm::mat4 baseMatrix = glm::mat4(1.0f);
        bool hasMatrix = false;
        glm::vec3 translation = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
    };

    struct Skin
    {
        std::vector<int> joints;
        std::vector<glm::mat4> inverseBindMatrices;
    };

    enum class ChannelPath
    {
        Translation,
        Rotation,
        Scale
    };

    enum class Interpolation
    {
        Linear,
        Step
    };

    struct Channel
    {
        int node = -1;
        ChannelPath path = ChannelPath::Translation;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float> times;
        std::vector<glm::vec4> values;
    };

    struct Animation
    {
        std::string name;
        std::vector<Channel> channels;
        float duration = 0.0f;
    };

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<NodePose> nodes;
    std::vector<int> sceneRoots;
    std::vector<Skin> skins;
    std::vector<Animation> animations;
    std::vector<glm::mat4> jointMatrices;
    int activeAnimation = -1;
    std::vector<unsigned char> embeddedBaseColorTexture;

    void resetPose()
    {
        for (NodePose& node : nodes)
        {
            node.translation = node.baseTranslation;
            node.rotation = node.baseRotation;
            node.scale = node.baseScale;
        }
    }

    void computeGlobals()
    {
        const auto visit = [&](auto&& self, int nodeIndex, const glm::mat4& parent) -> void
        {
            if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= nodes.size())
            {
                return;
            }

            NodePose& node = nodes[static_cast<std::size_t>(nodeIndex)];
            const glm::mat4 local = node.hasMatrix
                ? node.baseMatrix
                : composeTransform(node.translation, node.rotation, node.scale);
            node.global = parent * local;

            for (int child : node.children)
            {
                self(self, child, node.global);
            }
        };

        if (!sceneRoots.empty())
        {
            for (int root : sceneRoots)
            {
                visit(visit, root, glm::mat4(1.0f));
            }
            return;
        }

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].parent < 0)
            {
                visit(visit, static_cast<int>(i), glm::mat4(1.0f));
            }
        }
    }

    void updateJointMatrices()
    {
        if (skins.empty())
        {
            jointMatrices.clear();
            return;
        }

        const Skin& skin = skins.front();
        jointMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
        for (std::size_t i = 0; i < skin.joints.size(); ++i)
        {
            const int nodeIndex = skin.joints[i];
            const glm::mat4 inverseBind = i < skin.inverseBindMatrices.size()
                ? skin.inverseBindMatrices[i]
                : glm::mat4(1.0f);

            if (nodeIndex >= 0 && static_cast<std::size_t>(nodeIndex) < nodes.size())
            {
                jointMatrices[i] = nodes[static_cast<std::size_t>(nodeIndex)].global * inverseBind;
            }
            else
            {
                jointMatrices[i] = inverseBind;
            }
        }
    }
};

bool AnimatedModel::loadFromGlb(const std::string& path)
{
    destroy();
    impl_ = new Impl();

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success)
    {
        std::cerr << "Failed to parse GLB model: " << path << '\n';
        destroy();
        return false;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success)
    {
        std::cerr << "Failed to load GLB buffers: " << path << '\n';
        cgltf_free(data);
        destroy();
        return false;
    }

    for (cgltf_size i = 0; i < data->materials_count && impl_->embeddedBaseColorTexture.empty(); ++i)
    {
        const cgltf_texture* texture = data->materials[i].pbr_metallic_roughness.base_color_texture.texture;
        readEmbeddedImage(texture != nullptr ? texture->image : nullptr, impl_->embeddedBaseColorTexture);
    }
    for (cgltf_size i = 0; i < data->images_count && impl_->embeddedBaseColorTexture.empty(); ++i)
    {
        readEmbeddedImage(&data->images[i], impl_->embeddedBaseColorTexture);
    }

    impl_->nodes.resize(data->nodes_count);
    for (cgltf_size i = 0; i < data->nodes_count; ++i)
    {
        const cgltf_node& source = data->nodes[i];
        Impl::NodePose& node = impl_->nodes[static_cast<std::size_t>(i)];
        node.parent = source.parent != nullptr ? static_cast<int>(cgltf_node_index(data, source.parent)) : -1;
        node.baseTranslation = source.has_translation ? makeVec3(source.translation, glm::vec3(0.0f)) : glm::vec3(0.0f);
        node.baseRotation = source.has_rotation ? makeQuat(source.rotation) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        node.baseScale = source.has_scale ? makeVec3(source.scale, glm::vec3(1.0f)) : glm::vec3(1.0f);
        node.hasMatrix = source.has_matrix;
        node.baseMatrix = source.has_matrix ? makeMat4(source.matrix) : glm::mat4(1.0f);
        node.translation = node.baseTranslation;
        node.rotation = node.baseRotation;
        node.scale = node.baseScale;

        for (cgltf_size childIndex = 0; childIndex < source.children_count; ++childIndex)
        {
            node.children.push_back(static_cast<int>(cgltf_node_index(data, source.children[childIndex])));
        }
    }

    const cgltf_scene* scene = data->scene != nullptr ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
    if (scene != nullptr)
    {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i)
        {
            impl_->sceneRoots.push_back(static_cast<int>(cgltf_node_index(data, scene->nodes[i])));
        }
    }

    for (cgltf_size i = 0; i < data->skins_count; ++i)
    {
        const cgltf_skin& source = data->skins[i];
        Impl::Skin skin;
        for (cgltf_size jointIndex = 0; jointIndex < source.joints_count; ++jointIndex)
        {
            skin.joints.push_back(static_cast<int>(cgltf_node_index(data, source.joints[jointIndex])));
        }

        if (source.inverse_bind_matrices != nullptr)
        {
            for (cgltf_size matrixIndex = 0; matrixIndex < source.inverse_bind_matrices->count; ++matrixIndex)
            {
                float values[16] = {};
                cgltf_accessor_read_float(source.inverse_bind_matrices, matrixIndex, values, 16);
                skin.inverseBindMatrices.push_back(makeMat4(values));
            }
        }
        impl_->skins.push_back(skin);
    }

    for (cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex)
    {
        const cgltf_node& node = data->nodes[nodeIndex];
        if (node.mesh == nullptr)
        {
            continue;
        }

        for (cgltf_size primitiveIndex = 0; primitiveIndex < node.mesh->primitives_count; ++primitiveIndex)
        {
            const cgltf_primitive& primitive = node.mesh->primitives[primitiveIndex];
            if (primitive.type != cgltf_primitive_type_triangles)
            {
                continue;
            }

            const cgltf_accessor* positionAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            if (positionAccessor == nullptr)
            {
                continue;
            }

            const cgltf_accessor* normalAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
            const cgltf_accessor* texCoordAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* colorAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
            const cgltf_accessor* jointsAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
            const cgltf_accessor* weightsAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);
            const std::size_t firstVertex = impl_->vertices.size();

            for (cgltf_size vertexIndex = 0; vertexIndex < positionAccessor->count; ++vertexIndex)
            {
                Impl::Vertex vertex;

                float position[3] = {};
                if (cgltf_accessor_read_float(positionAccessor, vertexIndex, position, 3))
                {
                    vertex.position = glm::vec3(position[0], position[1], position[2]);
                }

                float normal[3] = {0.0f, 1.0f, 0.0f};
                if (normalAccessor != nullptr && cgltf_accessor_read_float(normalAccessor, vertexIndex, normal, 3))
                {
                    vertex.normal = glm::normalize(glm::vec3(normal[0], normal[1], normal[2]));
                }

                float texCoord[2] = {};
                if (texCoordAccessor != nullptr && cgltf_accessor_read_float(texCoordAccessor, vertexIndex, texCoord, 2))
                {
                    vertex.texCoord = glm::vec2(texCoord[0], texCoord[1]);
                }

                float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                if (colorAccessor != nullptr && cgltf_accessor_read_float(colorAccessor, vertexIndex, color, 4))
                {
                    vertex.color = glm::vec3(color[0], color[1], color[2]);
                }

                cgltf_uint joints[4] = {};
                if (jointsAccessor != nullptr && cgltf_accessor_read_uint(jointsAccessor, vertexIndex, joints, 4))
                {
                    vertex.joints = glm::uvec4(joints[0], joints[1], joints[2], joints[3]);
                }

                float weights[4] = {};
                if (weightsAccessor != nullptr && cgltf_accessor_read_float(weightsAccessor, vertexIndex, weights, 4))
                {
                    vertex.weights = glm::vec4(weights[0], weights[1], weights[2], weights[3]);
                }

                impl_->vertices.push_back(vertex);
            }

            if (primitive.indices != nullptr)
            {
                for (cgltf_size index = 0; index < primitive.indices->count; ++index)
                {
                    impl_->indices.push_back(static_cast<unsigned int>(firstVertex + cgltf_accessor_read_index(primitive.indices, index)));
                }
            }
        }
    }

    if (impl_->vertices.empty() || impl_->indices.empty())
    {
        std::cerr << "GLB model has no drawable mesh data: " << path << '\n';
        cgltf_free(data);
        destroy();
        return false;
    }

    for (cgltf_size animationIndex = 0; animationIndex < data->animations_count; ++animationIndex)
    {
        const cgltf_animation& source = data->animations[animationIndex];
        Impl::Animation animation;
        animation.name = source.name != nullptr ? source.name : "animation";

        for (cgltf_size channelIndex = 0; channelIndex < source.channels_count; ++channelIndex)
        {
            const cgltf_animation_channel& sourceChannel = source.channels[channelIndex];
            if (sourceChannel.target_node == nullptr || sourceChannel.sampler == nullptr)
            {
                continue;
            }

            Impl::Channel channel;
            channel.node = static_cast<int>(cgltf_node_index(data, sourceChannel.target_node));
            channel.interpolation = sourceChannel.sampler->interpolation == cgltf_interpolation_type_step
                ? Impl::Interpolation::Step
                : Impl::Interpolation::Linear;

            if (sourceChannel.target_path == cgltf_animation_path_type_rotation)
            {
                channel.path = Impl::ChannelPath::Rotation;
            }
            else if (sourceChannel.target_path == cgltf_animation_path_type_scale)
            {
                channel.path = Impl::ChannelPath::Scale;
            }
            else if (sourceChannel.target_path == cgltf_animation_path_type_translation)
            {
                channel.path = Impl::ChannelPath::Translation;
            }
            else
            {
                continue;
            }

            const cgltf_accessor* input = sourceChannel.sampler->input;
            const cgltf_accessor* output = sourceChannel.sampler->output;
            if (input == nullptr || output == nullptr)
            {
                continue;
            }

            for (cgltf_size timeIndex = 0; timeIndex < input->count; ++timeIndex)
            {
                float value = 0.0f;
                if (cgltf_accessor_read_float(input, timeIndex, &value, 1))
                {
                    channel.times.push_back(value);
                    animation.duration = std::max(animation.duration, value);
                }
            }

            const int componentCount = channel.path == Impl::ChannelPath::Rotation ? 4 : 3;
            for (cgltf_size valueIndex = 0; valueIndex < output->count; ++valueIndex)
            {
                float values[4] = {0.0f, 0.0f, 0.0f, channel.path == Impl::ChannelPath::Rotation ? 1.0f : 0.0f};
                if (cgltf_accessor_read_float(output, valueIndex, values, componentCount))
                {
                    channel.values.emplace_back(values[0], values[1], values[2], values[3]);
                }
            }

            if (!channel.times.empty() && !channel.values.empty())
            {
                animation.channels.push_back(channel);
            }
        }

        if (!animation.channels.empty())
        {
            impl_->animations.push_back(animation);
        }
    }

    setActiveAnimation("spongebob_idle01.anm");
    if (impl_->activeAnimation < 0 && !impl_->animations.empty())
    {
        impl_->activeAnimation = 0;
    }

    cgltf_free(data);

    indexCount_ = static_cast<GLsizei>(impl_->indices.size());
    gpuVertices_.resize(impl_->vertices.size() * kVertexStride, 0.0f);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(gpuVertices_.size() * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->indices.size() * sizeof(unsigned int)), impl_->indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), reinterpret_cast<void*>(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);

    updateAnimation(0.0f);
    return true;
}

bool AnimatedModel::setActiveAnimation(const std::string& animationName)
{
    if (impl_ == nullptr)
    {
        return false;
    }

    for (std::size_t i = 0; i < impl_->animations.size(); ++i)
    {
        if (impl_->animations[i].name == animationName)
        {
            impl_->activeAnimation = static_cast<int>(i);
            return true;
        }
    }

    return false;
}

void AnimatedModel::updateAnimation(float elapsedTime)
{
    if (!isLoaded() || impl_ == nullptr)
    {
        return;
    }

    const Impl::Animation* animation =
        impl_->activeAnimation >= 0 && static_cast<std::size_t>(impl_->activeAnimation) < impl_->animations.size()
            ? &impl_->animations[static_cast<std::size_t>(impl_->activeAnimation)]
            : nullptr;

    impl_->resetPose();

    const float duration = animation != nullptr ? animation->duration : 0.0f;
    const float time = duration > 0.0f ? std::fmod(elapsedTime, duration) : 0.0f;

    if (animation != nullptr)
    {
        for (const Impl::Channel& channel : animation->channels)
        {
            if (channel.node < 0 || static_cast<std::size_t>(channel.node) >= impl_->nodes.size() || channel.times.empty() || channel.values.empty())
            {
                continue;
            }

            std::size_t upper = std::upper_bound(channel.times.begin(), channel.times.end(), time) - channel.times.begin();
            if (upper == 0)
            {
                upper = 1;
            }
            if (upper >= channel.times.size())
            {
                upper = channel.times.size() - 1;
            }

            const std::size_t lower = upper - 1;
            const float start = channel.times[lower];
            const float end = channel.times[upper];
            const float factor = channel.interpolation == Impl::Interpolation::Step || end <= start
                ? 0.0f
                : std::clamp((time - start) / (end - start), 0.0f, 1.0f);

            const glm::vec4 a = channel.values[std::min(lower, channel.values.size() - 1)];
            const glm::vec4 b = channel.values[std::min(upper, channel.values.size() - 1)];
            Impl::NodePose& node = impl_->nodes[static_cast<std::size_t>(channel.node)];

            if (channel.path == Impl::ChannelPath::Rotation)
            {
                const glm::quat qa = glm::normalize(glm::quat(a.w, a.x, a.y, a.z));
                const glm::quat qb = glm::normalize(glm::quat(b.w, b.x, b.y, b.z));
                node.rotation = glm::normalize(glm::slerp(qa, qb, factor));
            }
            else
            {
                const glm::vec3 value = glm::mix(glm::vec3(a), glm::vec3(b), factor);
                if (channel.path == Impl::ChannelPath::Translation)
                {
                    node.translation = value;
                }
                else
                {
                    node.scale = value;
                }
            }
        }
    }

    impl_->computeGlobals();
    impl_->updateJointMatrices();

    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    for (std::size_t i = 0; i < impl_->vertices.size(); ++i)
    {
        const Impl::Vertex& source = impl_->vertices[i];
        glm::vec4 skinnedPosition(0.0f);
        glm::vec3 skinnedNormal(0.0f);
        float totalWeight = 0.0f;

        for (int jointSlot = 0; jointSlot < 4; ++jointSlot)
        {
            const float weight = source.weights[static_cast<std::size_t>(jointSlot)];
            if (weight <= 0.0f)
            {
                continue;
            }

            const unsigned int jointIndex = source.joints[static_cast<std::size_t>(jointSlot)];
            if (jointIndex >= impl_->jointMatrices.size())
            {
                continue;
            }

            const glm::mat4& jointMatrix = impl_->jointMatrices[jointIndex];
            skinnedPosition += weight * (jointMatrix * glm::vec4(source.position, 1.0f));
            skinnedNormal += weight * (glm::mat3(jointMatrix) * source.normal);
            totalWeight += weight;
        }

        if (totalWeight <= 0.0f)
        {
            skinnedPosition = glm::vec4(source.position, 1.0f);
            skinnedNormal = source.normal;
        }
        else if (std::abs(totalWeight - 1.0f) > 0.0001f)
        {
            skinnedPosition /= totalWeight;
            skinnedNormal /= totalWeight;
        }

        const glm::vec3 position(skinnedPosition);
        const glm::vec3 normal = glm::length(skinnedNormal) > 0.0001f
            ? glm::normalize(skinnedNormal)
            : glm::vec3(0.0f, 1.0f, 0.0f);

        minBounds = glm::min(minBounds, position);
        maxBounds = glm::max(maxBounds, position);

        const std::size_t base = i * kVertexStride;
        gpuVertices_[base + 0] = position.x;
        gpuVertices_[base + 1] = position.y;
        gpuVertices_[base + 2] = position.z;
        gpuVertices_[base + 3] = normal.x;
        gpuVertices_[base + 4] = normal.y;
        gpuVertices_[base + 5] = normal.z;
        gpuVertices_[base + 6] = source.texCoord.x;
        gpuVertices_[base + 7] = source.texCoord.y;
        gpuVertices_[base + 8] = source.color.r;
        gpuVertices_[base + 9] = source.color.g;
        gpuVertices_[base + 10] = source.color.b;
    }

    minBounds_ = minBounds;
    maxBounds_ = maxBounds;

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(gpuVertices_.size() * sizeof(float)), gpuVertices_.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void AnimatedModel::draw() const
{
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

bool AnimatedModel::hasEmbeddedBaseColorTexture() const
{
    return impl_ != nullptr && !impl_->embeddedBaseColorTexture.empty();
}

const std::vector<unsigned char>& AnimatedModel::embeddedBaseColorTexture() const
{
    static const std::vector<unsigned char> emptyTexture;
    return impl_ != nullptr ? impl_->embeddedBaseColorTexture : emptyTexture;
}

void AnimatedModel::destroy()
{
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
    vbo_ = 0;
    ebo_ = 0;
    indexCount_ = 0;
    minBounds_ = glm::vec3(0.0f);
    maxBounds_ = glm::vec3(0.0f);
    gpuVertices_.clear();
    delete impl_;
    impl_ = nullptr;
}
