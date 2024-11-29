#pragma once

class Camera;

class IRendererModule
{
public:
    /** Call from the game thread to send a message to the rendering thread to being rendering this view family. */
    virtual void BeginRenderingViewFamily(Camera *ViewFamily) = 0;
};