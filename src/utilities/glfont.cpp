#include <iostream>
#include "glfont.h"

Mesh generateTextGeometryBuffer(std::string text, float characterHeightOverWidth, float totalTextWidth) {
    float characterWidth = totalTextWidth / float(text.length());
    float characterHeight = characterHeightOverWidth * characterWidth;

    unsigned int vertexCount = 4 * text.length();
    unsigned int indexCount = 6 * text.length();

    Mesh mesh;

    mesh.vertices.resize(vertexCount);
    mesh.indices.resize(indexCount);
    mesh.textureCoordinates.resize(vertexCount);


    for(unsigned int i = 0; i < text.length(); i++)
    {
        float baseXCoordinate = float(i) * characterWidth;

        mesh.vertices.at(4 * i + 0) = {baseXCoordinate, 0, 0};                              // bottom-left
        mesh.vertices.at(4 * i + 1) = {baseXCoordinate + characterWidth, 0, 0};             // bottom-right
        mesh.vertices.at(4 * i + 2) = {baseXCoordinate + characterWidth, characterHeight, 0}; // top-right
        mesh.vertices.at(4 * i + 3) = {baseXCoordinate, characterHeight, 0}; 


        mesh.indices.at(6 * i + 0) = 4 * i + 0;
        mesh.indices.at(6 * i + 1) = 4 * i + 1;
        mesh.indices.at(6 * i + 2) = 4 * i + 2;
        mesh.indices.at(6 * i + 3) = 4 * i + 0;
        mesh.indices.at(6 * i + 4) = 4 * i + 2;
        mesh.indices.at(6 * i + 5) = 4 * i + 3;

        float u_left = float(text[i])/128.0f;
        float u_right = float(text[i]+1)/128.0f;
        float v_down = 0.0f;
        float v_up = 1.0f;
        

        mesh.textureCoordinates.at(4*i + 0) = {u_left,  v_down}; //bottom-left
        mesh.textureCoordinates.at(4*i + 1) = {u_right, v_down}; //bottom-right
        mesh.textureCoordinates.at(4*i + 2) = {u_right, v_up}; //top-right
        mesh.textureCoordinates.at(4*i + 3) = {u_left,  v_up}; //top-left


    }

    

    return mesh;
}