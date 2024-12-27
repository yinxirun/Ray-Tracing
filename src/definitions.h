#pragma once
#include <cstdint>
#include <cassert>

/*** 尹喜润 ***/
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
/** NEED TO RENAME, for RT version of GFrameTime use View.ViewFamily->FrameNumber or pass down from RT from GFrameTime). */
#define VK_DESCRIPTOR_TYPE_BEGIN_RANGE VK_DESCRIPTOR_TYPE_SAMPLER
#define VK_DESCRIPTOR_TYPE_END_RANGE VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT

/// 不区分工作线程、渲染线程和RHI线程，所以全部返回true
inline bool IsInGameThread() { return true; }
/// 不区分工作线程、渲染线程和RHI线程，所以全部返回true
inline bool IsInRenderingThread() { return true; }
/// 不区分工作线程、渲染线程和RHI线程，所以全部返回true
inline bool IsInRHIThread() { return true; }

/*----------------------------------------------------------------------------
    TList.
----------------------------------------------------------------------------*/

//
// Simple single-linked list template.
//
template <class ElementType>
class TList
{
public:
    ElementType Element;
    TList<ElementType> *Next;

    // Constructor.

    TList(const ElementType &InElement, TList<ElementType> *InNext = nullptr)
    {
        Element = InElement;
        Next = InNext;
    }
};


// 用于控制是否打印未实现函数的信息
#define PRINT_UNIMPLEMENT
#undef PRINT_UNIMPLEMENT

#define PRINT_SEPERATOR_RHI_UNIMPLEMENT
#undef PRINT_SEPERATOR_RHI_UNIMPLEMENT

#define PRINT_TO_EXPLORE
#undef PRINT_TO_EXPLORE

#define DEBUG_BUFFER_CREATE_DESTROY
#undef DEBUG_BUFFER_CREATE_DESTROY