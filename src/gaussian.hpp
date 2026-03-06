#pragma once

#include <vector>
#include <pcl/io/ply_io.h>
#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>


struct GaussianData {
    glm::vec3 position;
    glm::vec3 scale;
    glm::vec4 rotation;
    float opacity;

    //TODO: add in rest of lighting fields after working prototype
};

class GaussianLoader {
    private:
        // pcl classes for loading ply files
        pcl::PCLPointCloud2 point_cloud;
        pcl::PLYReader ply_reader;

        // The path to the .ply file containing the gaussian splat data
        std::string plyFilePath;

        std::vector<GaussianData> gaussianSplats;

        //helper func to find offsets
        int findField(const std::string& name)
            {
                for (int i = 0; i < point_cloud.fields.size(); i++)
                {
                    if (point_cloud.fields[i].name == name)
                        return i;
                }

                throw std::runtime_error("Field not found: " + name);
            }
        
    public:
        GaussianLoader (std::string plyFilePath) {
            this->plyFilePath = plyFilePath;
            // Reading the ply file
            if (ply_reader.read(plyFilePath, point_cloud) < 0) {
                throw std::runtime_error("Failed to load PLY");
            }
            
            //getting indices for the fields
            int x_i = findField("x");
            int y_i = findField("y");
            int z_i = findField("z");

            int scale0_i = findField("scale_0");
            int scale1_i = findField("scale_1");
            int scale2_i = findField("scale_2");

            int rot0_i = findField("rot_0");
            int rot1_i = findField("rot_1");
            int rot2_i = findField("rot_2");
            int rot3_i = findField("rot_3");

            int opacity_i = findField("opacity");

            auto& fields = point_cloud.fields;

            size_t x_offset = fields[x_i].offset;
            size_t y_offset = fields[y_i].offset;
            size_t z_offset = fields[z_i].offset;

            size_t scale0_offset = fields[scale0_i].offset;
            size_t scale1_offset = fields[scale1_i].offset;
            size_t scale2_offset = fields[scale2_i].offset;

            size_t rot0_offset = fields[rot0_i].offset;
            size_t rot1_offset = fields[rot1_i].offset;
            size_t rot2_offset = fields[rot2_i].offset;
            size_t rot3_offset = fields[rot3_i].offset;

            size_t opacity_offset = fields[opacity_i].offset;


            int pointCount = point_cloud.width * point_cloud.height;
            int stride = point_cloud.point_step;

            gaussianSplats.reserve(pointCount);

            const uint8_t* data = point_cloud.data.data();

            for (int i = 0; i < pointCount; i++)
            {
                const uint8_t* point = data + i * stride;

                GaussianData g;

                g.position = glm::vec3(
                    *reinterpret_cast<const float*>(point + x_offset),
                    *reinterpret_cast<const float*>(point + y_offset),
                    *reinterpret_cast<const float*>(point + z_offset)
                );

                g.scale = glm::vec3(
                    *reinterpret_cast<const float*>(point + scale0_offset),
                    *reinterpret_cast<const float*>(point + scale1_offset),
                    *reinterpret_cast<const float*>(point + scale2_offset)
                );

                g.rotation = glm::vec4(
                    *reinterpret_cast<const float*>(point + rot0_offset),
                    *reinterpret_cast<const float*>(point + rot1_offset),
                    *reinterpret_cast<const float*>(point + rot2_offset),
                    *reinterpret_cast<const float*>(point + rot3_offset)
                );

                g.opacity = *reinterpret_cast<const float*>(point + opacity_offset);

                gaussianSplats.push_back(g);
            }

        }

        std::vector<GaussianData>* getGaussianSplats() {
            return &gaussianSplats;
        }
    
};