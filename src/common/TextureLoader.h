#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <string>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

namespace TextureLoader {
    // 이미지를 로딩하고 OpenGL 텍스처 ID를 반환하는 함수
    // 성공하면 true, 실패하면 false 반환
    bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
        // 이미지 로딩
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
        if (image_data == NULL) {
            std::printf("Failed to load texture: %s\n", filename);
            return false;
        }

        // OpenGL 텍스처 생성 및 설정
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // 텍스처 필터링 설정 (밉맵 활성화)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 텍스처 가장자리 처리
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

        // 업로드
#if defined(GL_UNPACK_ROW_LENGTH)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

        stbi_image_free(image_data);

        *out_texture = image_texture;
        *out_width = image_width;
        *out_height = image_height;

        return true;
    }
}