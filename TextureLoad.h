#pragma once
#include <Novice.h>
#include <cstdio>

// 画像がない時は-1。呼び出し元のstaticハンドルでロード結果を保持する。
namespace TextureLoad {
    inline int Optional(const char* path) {
        FILE* file = nullptr;
        if (fopen_s(&file, path, "rb") != 0 || !file) return -1;
        fclose(file);
        return Novice::LoadTexture(path);
    }
}
