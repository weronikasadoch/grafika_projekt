#pragma once

#include <glm.hpp>
#include <vector>
#include <cmath>

class CurvePathGenerator
{
public:
	struct PathPoint
	{
		glm::vec3 position;
		glm::vec3 tangent;
		glm::vec3 normal;
		glm::vec3 binormal;
		float t;
	};

	static std::vector<PathPoint> generateBezierPathWithPTF(
		const glm::vec3& p0,
		const glm::vec3& p1,
		const glm::vec3& p2,
		const glm::vec3& p3,
		int segments = 50)
	{
		std::vector<PathPoint> path;
		path.reserve(segments + 1);

		for (int i = 0; i <= segments; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(segments);
			PathPoint point;
			point.t = t;
			point.position = evaluateBezier(p0, p1, p2, p3, t);
			point.tangent = glm::normalize(evaluateBezierDerivative(p0, p1, p2, p3, t));
			path.push_back(point);
		}

		if (!path.empty())
		{
			glm::vec3 initialNormal = computeInitialNormal(path[0].tangent);
			path[0].normal = initialNormal;
			path[0].binormal = glm::normalize(glm::cross(path[0].tangent, path[0].normal));
		}

		for (size_t i = 1; i < path.size(); ++i)
		{
			path[i].normal = parallelTransportFrame(
				path[i - 1].tangent,
				path[i].tangent,
				path[i - 1].normal
			);
			path[i].binormal = glm::normalize(glm::cross(path[i].tangent, path[i].normal));
		}

		return path;
	}

	static std::vector<PathPoint> generateSpiralPathWithPTF(
		const glm::vec3& startPos,
		float height,
		float radius,
		float turns,
		int segments = 50)
	{
		std::vector<PathPoint> path;
		path.reserve(segments + 1);

		for (int i = 0; i <= segments; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(segments);
			float angle = t * turns * 2.0f * 3.14159265359f;
			float y = startPos.y + t * height;

			PathPoint point;
			point.t = t;
			point.position = glm::vec3(
				startPos.x + radius * std::cos(angle),
				y,
				startPos.z + radius * std::sin(angle)
			);

			glm::vec3 tangentDir(
				-radius * std::sin(angle),
				height / segments,
				radius * std::cos(angle)
			);
			point.tangent = glm::normalize(tangentDir);

			path.push_back(point);
		}

		if (!path.empty())
		{
			glm::vec3 initialNormal = computeInitialNormal(path[0].tangent);
			path[0].normal = initialNormal;
			path[0].binormal = glm::normalize(glm::cross(path[0].tangent, path[0].normal));
		}

		for (size_t i = 1; i < path.size(); ++i)
		{
			path[i].normal = parallelTransportFrame(
				path[i - 1].tangent,
				path[i].tangent,
				path[i - 1].normal
			);
			path[i].binormal = glm::normalize(glm::cross(path[i].tangent, path[i].normal));
		}

		return path;
	}


	static PathPoint interpolatePath(const std::vector<PathPoint>& path, float t)
	{
		if (path.empty())
		{
			return PathPoint();
		}

		t = glm::clamp(t, 0.0f, 1.0f);
		float segmentFloat = t * static_cast<float>(path.size() - 1);
		size_t segment = static_cast<size_t>(segmentFloat);

		if (segment >= path.size() - 1)
		{
			return path.back();
		}

		float localT = segmentFloat - static_cast<float>(segment);

		PathPoint result;
		result.position = glm::mix(path[segment].position, path[segment + 1].position, localT);
		result.tangent = glm::normalize(glm::mix(path[segment].tangent, path[segment + 1].tangent, localT));
		result.normal = glm::normalize(glm::mix(path[segment].normal, path[segment + 1].normal, localT));
		result.binormal = glm::normalize(glm::cross(result.tangent, result.normal));
		result.t = t;

		return result;
	}

private:
	static glm::vec3 evaluateBezier(
		const glm::vec3& p0,
		const glm::vec3& p1,
		const glm::vec3& p2,
		const glm::vec3& p3,
		float t)
	{
		float u = 1.0f - t;
		float tt = t * t;
		float uu = u * u;
		float uuu = uu * u;
		float ttt = tt * t;

		return uuu * p0 +
			   3.0f * uu * t * p1 +
			   3.0f * u * tt * p2 +
			   ttt * p3;
	}
	static glm::vec3 evaluateBezierDerivative(
		const glm::vec3& p0,
		const glm::vec3& p1,
		const glm::vec3& p2,
		const glm::vec3& p3,
		float t)
	{
		float u = 1.0f - t;
		float tt = t * t;
		float uu = u * u;

		return 3.0f * uu * (p1 - p0) +
			   6.0f * u * t * (p2 - p1) +
			   3.0f * tt * (p3 - p2);
	}

	static glm::vec3 computeInitialNormal(const glm::vec3& tangent)
	{
		glm::vec3 up(0.0f, 1.0f, 0.0f);

		if (std::abs(glm::dot(tangent, up)) > 0.99f)
		{
			up = glm::vec3(1.0f, 0.0f, 0.0f);
		}

		glm::vec3 binormal = glm::normalize(glm::cross(tangent, up));
		return glm::normalize(glm::cross(binormal, tangent));
	}
	static glm::vec3 parallelTransportFrame(
		const glm::vec3& prevTangent,
		const glm::vec3& currTangent,
		const glm::vec3& prevNormal)
	{
		glm::vec3 axis = glm::cross(prevTangent, currTangent);
		float axisLength = glm::length(axis);
		if (axisLength < 0.0001f)
		{
			return prevNormal;
		}

		axis /= axisLength;
		float angle = std::acos(glm::clamp(glm::dot(prevTangent, currTangent), -1.0f, 1.0f));
		float cosAngle = std::cos(angle);
		float sinAngle = std::sin(angle);

		glm::vec3 rotated = 
			prevNormal * cosAngle +
			glm::cross(axis, prevNormal) * sinAngle +
			axis * glm::dot(axis, prevNormal) * (1.0f - cosAngle);

		return glm::normalize(rotated);
	}
};
