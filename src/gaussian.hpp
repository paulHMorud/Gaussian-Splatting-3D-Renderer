#pragma once

#include <vector>
#include <array>
#include <string>
#include <stdexcept>
#include <cstddef>
#include <glm/glm.hpp>
#include <pcl/io/ply_io.h>
#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>

// Struct for holding the data of a single splat
// Should be sent straight to the GPU when I implement spherical harmonics. 
struct GaussianData {
    glm::vec3 position;     // x, y, z
    // glm::vec3 normal;       // normal_x, normal_y, normal_z

    glm::vec3 f_dc;         // f_dc_0, f_dc_1, f_dc_2
    std::array<float, 45> f_rest; // f_rest_0 ... f_rest_44

    float opacity;          // opacity

    glm::vec3 scale;        // scale_0, scale_1, scale_2
    glm::vec4 rotation;     // rot_0, rot_1, rot_2, rot_3
};


// To be honest this should in no way be its own class.
// Just wanted to see how to create classes in C++, should obviously just be a single function. 
class GaussianLoader {
private:
    pcl::PCLPointCloud2 point_cloud;
    pcl::PLYReader ply_reader;

    std::string plyFilePath;
    std::vector<GaussianData> gaussianSplats;

    int findField(const std::string& name) const
    {
        for (size_t i = 0; i < point_cloud.fields.size(); i++) {
            if (point_cloud.fields[i].name == name) {
                return static_cast<int>(i);
            }
        }

        throw std::runtime_error("Field not found: " + name);
    }

    static float readFloat(const uint8_t* point, size_t offset)
    {
        return *reinterpret_cast<const float*>(point + offset);
    }

public:
    GaussianLoader(const std::string& plyFilePath)
        : plyFilePath(plyFilePath)
    {
        if (ply_reader.read(plyFilePath, point_cloud) < 0) {
            throw std::runtime_error("Failed to load PLY");
        }

        auto& fields = point_cloud.fields;

        // Required field indices
        int x_i = findField("x");
        int y_i = findField("y");
        int z_i = findField("z");

        // int nx_i = findField("normal_x");
        // int ny_i = findField("normal_y");
        // int nz_i = findField("normal_z");

        int fdc0_i = findField("f_dc_0");
        int fdc1_i = findField("f_dc_1");
        int fdc2_i = findField("f_dc_2");

        int opacity_i = findField("opacity");

        int scale0_i = findField("scale_0");
        int scale1_i = findField("scale_1");
        int scale2_i = findField("scale_2");

        int rot0_i = findField("rot_0");
        int rot1_i = findField("rot_1");
        int rot2_i = findField("rot_2");
        int rot3_i = findField("rot_3");

        // Offsets
        size_t x_offset = fields[x_i].offset;
        size_t y_offset = fields[y_i].offset;
        size_t z_offset = fields[z_i].offset;

        // size_t nx_offset = fields[nx_i].offset;
        // size_t ny_offset = fields[ny_i].offset;
        // size_t nz_offset = fields[nz_i].offset;

        size_t fdc0_offset = fields[fdc0_i].offset;
        size_t fdc1_offset = fields[fdc1_i].offset;
        size_t fdc2_offset = fields[fdc2_i].offset;

        size_t opacity_offset = fields[opacity_i].offset;

        size_t scale0_offset = fields[scale0_i].offset;
        size_t scale1_offset = fields[scale1_i].offset;
        size_t scale2_offset = fields[scale2_i].offset;

        size_t rot0_offset = fields[rot0_i].offset;
        size_t rot1_offset = fields[rot1_i].offset;
        size_t rot2_offset = fields[rot2_i].offset;
        size_t rot3_offset = fields[rot3_i].offset;

        // f_rest offsets
        std::array<size_t, 45> f_rest_offsets{};
        for (int i = 0; i < 45; i++) {
            int idx = findField("f_rest_" + std::to_string(i));
            f_rest_offsets[i] = fields[idx].offset;
        }

        size_t pointCount = static_cast<size_t>(point_cloud.width) * static_cast<size_t>(point_cloud.height);
        size_t stride = point_cloud.point_step;

        gaussianSplats.reserve(pointCount);

        const uint8_t* data = point_cloud.data.data();

        for (size_t i = 0; i < pointCount; i++) {
            const uint8_t* point = data + i * stride;

            GaussianData g{};

            g.position = glm::vec3(
                -readFloat(point, x_offset),
                -readFloat(point, y_offset),
                readFloat(point, z_offset)
            );

            // g.normal = glm::vec3(
            //     -readFloat(point, nx_offset),
            //     -readFloat(point, ny_offset),
            //     readFloat(point, nz_offset)
            // );

            g.f_dc = glm::vec3(
                readFloat(point, fdc0_offset),
                readFloat(point, fdc1_offset),
                readFloat(point, fdc2_offset)
            );

            for (int j = 0; j < 45; j++) {
                g.f_rest[j] = readFloat(point, f_rest_offsets[j]);
            }

            g.opacity = readFloat(point, opacity_offset);

            g.scale = glm::exp(
                glm::vec3(
                readFloat(point, scale0_offset),
                readFloat(point, scale1_offset),
                readFloat(point, scale2_offset)
                )
            );

            // Took way to long to realize I have to normalize to get valid rotation
            // Why they dont just save the data in a normalized format is beyond me
            g.rotation = glm::normalize( 
                glm::vec4(
                    readFloat(point, rot0_offset),
                    -readFloat(point, rot1_offset),
                    -readFloat(point, rot2_offset),
                    readFloat(point, rot3_offset)
                )
            );
    


            gaussianSplats.push_back(g);
        }
        
    }

    std::vector<GaussianData>& getGaussianSplats()
    {
        return gaussianSplats;
    }

    const std::vector<GaussianData>& getGaussianSplats() const
    {
        return gaussianSplats;
    }
};