#pragma once
#include "RHI/RHIResources.h"

struct VertexInputStream
{
    uint32 StreamIndex : 4;
    uint32 Offset : 28;
    Buffer *VertexBuffer;

    VertexInputStream() : StreamIndex(0),
                          Offset(0),
                          VertexBuffer(nullptr) {}

    VertexInputStream(uint32 InStreamIndex, uint32 InOffset, Buffer *InVertexBuffer)
        : StreamIndex(InStreamIndex), Offset(InOffset), VertexBuffer(InVertexBuffer)
    {
        // Verify no overflow
        check(InStreamIndex == StreamIndex && InOffset == Offset);
    }

    inline bool operator==(const VertexInputStream &rhs) const
    {
        if (StreamIndex != rhs.StreamIndex ||
            Offset != rhs.Offset ||
            VertexBuffer != rhs.VertexBuffer)
        {
            return false;
        }

        return true;
    }

    inline bool operator!=(const VertexInputStream &rhs) const
    {
        return !(*this == rhs);
    }
};

/// Encapsulates a vertex data source which can be linked into a vertex shader.
class VertexFactory
{
public:
    const std::shared_ptr<VertexDeclaration> &GetDeclaration() const { return declaration; }

protected:
    /// Information needed to set a vertex stream.
    struct VertexStream
    {
        const Buffer *VertexBuffer = nullptr;
        uint32 Offset = 0;
        uint16 Stride = 0;
        uint8 Padding = 0;

        friend bool operator==(const VertexStream &A, const VertexStream &B)
        {
            return A.VertexBuffer == B.VertexBuffer && A.Stride == B.Stride && A.Offset == B.Offset;
        }

        VertexStream() {}
    };
    /** The vertex streams used to render the factory. */
    std::vector<VertexStream> Streams;

private:
    std::shared_ptr<VertexDeclaration> declaration;

    // /** The position only vertex stream used to render the factory during depth only passes. */
    // TArray<FVertexStream, TInlineAllocator<2>> PositionStream;
    // TArray<FVertexStream, TInlineAllocator<3>> PositionAndNormalStream;

    // /** The RHI vertex declaration used to render the factory during depth only passes. */
    // FVertexDeclarationRHIRef PositionDeclaration;
    // FVertexDeclarationRHIRef PositionAndNormalDeclaration;
};