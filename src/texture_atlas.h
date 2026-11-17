#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize(size) {}

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        auto position = mOffset;
        float rowHeight = mRowHeight;
        if (position.x + rectSize.x > mSize.x) {
            position = {0.f, position.y + rowHeight};
            rowHeight = 0.f;
        }
        if (rectSize.x > mSize.x || position.y + rectSize.y > mSize.y) {
            return {false, std::numeric_limits<size_t>::max()};
        }

        mBounds.push_back({position, rectSize});
        mOffset = {position.x + rectSize.x, position.y};
        mRowHeight = rowHeight > rectSize.y ? rowHeight : rectSize.y;
        return {true, mBounds.size() - 1};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const {
        assert(rectID < mBounds.size());
        return mBounds[rectID];
    }

private:
    sf::Vector2f mSize;
    sf::Vector2f mOffset{};
    float mRowHeight = 0.f;
    std::vector<sf::FloatRect> mBounds;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, uint pixels) {
    const auto size = img.getSize();
    const sf::Vector2u paddedSize{size.x + 2 * pixels, size.y + 2 * pixels};
    sf::Image padded(paddedSize);
    for (uint y = 0; y < paddedSize.y; ++y) {
        uint sourceY = y < pixels ? 0 : y - pixels;
        if (sourceY >= size.y) {
            sourceY = size.y - 1;
        }
        for (uint x = 0; x < paddedSize.x; ++x) {
            uint sourceX = x < pixels ? 0 : x - pixels;
            if (sourceX >= size.x) {
                sourceX = size.x - 1;
            }
            padded.setPixel({x, y}, img.getPixel({sourceX, sourceY}));
        }
    }
    return padded;
}

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        const auto atlasSize = mTexture.getSize();
        if (mPadding > atlasSize.x / 2 || mPadding > atlasSize.y / 2) {
            return std::numeric_limits<TextureID>::max();
        }
        const auto padded = padImage(sf::Image(filepath), static_cast<uint>(mPadding));
        const auto result = mPacker.pack(sf::Vector2f(padded.getSize()));
        if (result.first) {
            mTexture.update(padded, sf::Vector2u(mPacker.getBounds(result.second).position));
        }
        return result.second;
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        const auto bounds = mPacker.getBounds(id);
        const sf::Vector2f atlasSize(mTexture.getSize());
        const float padding = static_cast<float>(mPadding);
        const sf::Vector2f bottomLeft{bounds.position.x + padding,
                                      atlasSize.y - (bounds.position.y + bounds.size.y - padding)};
        return {bottomLeft / atlasSize, sf::Vector2f(getSizePixels(id)) / atlasSize};
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        const uint border = static_cast<uint>(2 * mPadding);
        return sf::Vector2u(mPacker.getBounds(id).size) - sf::Vector2u(border, border);
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
