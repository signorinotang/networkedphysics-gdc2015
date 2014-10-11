#include "Console.h"
#include "Global.h"
#include "Font.h"
#include "FontManager.h"
#include "ShaderManager.h"
#include "core/Core.h"
#include "core/Memory.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::mat4;
using glm::vec3;
using glm::vec4;

static const int MaxLine = 256;
static const int CommandHistorySize = 256;

struct ConsoleInternal
{
    ConsoleInternal()
    {
        vao = 0;
        vbo = 0;

        active = false;
        justActivated = false;
        justDeactivated = false;

        commandHistoryIsEmpty = true;
        commandCursorPosition = 0;
        commandLength = 0;
        commandHistoryIndex = 0;
        commandHistorySelection = -1;
        memset( commandString, 0, sizeof( commandString ) );
        memset( commandHistory, 0, sizeof( commandHistory ) );
    }

    ~ConsoleInternal()
    {
        if ( vao )
        {
            glDeleteBuffers( 1, &vao );
            vao = 0;
        }

        if ( vbo )
        {
            glDeleteBuffers( 1, &vbo );
            vbo = 0;
        }
    }

    GLuint vao, vbo;

    bool active;
    bool justActivated;
    bool justDeactivated;

    bool commandHistoryIsEmpty;
    int commandCursorPosition;
    int commandLength;
    int commandHistoryIndex;
    int commandHistorySelection;
    char commandString[MaxLine];
    char commandHistory[CommandHistorySize][MaxLine];

    void Activate( bool eatNextKey = false )
    {
        active = true;
        justActivated = eatNextKey;
    }

    void Deactivate( bool eatNextKey = false )
    {
        active = false;
        justDeactivated = eatNextKey;
    }

    void ClearCommandString()
    {
        commandLength = 0;
        commandCursorPosition = 0;
        memset( commandString, 0, sizeof( commandString ) );
        commandHistorySelection = -1;
    }

    void CharacterTyped( char c )
    {
        if ( commandLength < sizeof( commandString ) - 1 )
        {
            if ( commandCursorPosition < commandLength )
            {
                for ( int i = commandLength; i > commandCursorPosition; --i )
                    commandString[i] = commandString[i-1];
            }

            commandString[commandCursorPosition] = c;

            commandCursorPosition++;
            commandLength++;
        }
    }

    void Backspace()
    {
        if ( commandLength == 0 )
            return;

        if ( commandCursorPosition == 0 )
            return;

        if ( commandCursorPosition < commandLength )
        {
            for ( int i = commandCursorPosition; i < commandLength; ++i )
                commandString[i-1] = commandString[i];
        }

        commandString[commandLength-1] = '\0';

        --commandLength;
        --commandCursorPosition;
    }

    void CursorLeft()
    {
        if ( commandCursorPosition > 0 )
            commandCursorPosition--;
    }

    void CursorRight()
    {
        if ( commandCursorPosition < commandLength )
            commandCursorPosition++;
    }

    void CursorBegin()
    {
        commandCursorPosition = 0;
    }

    void CursorEnd()
    {
        commandCursorPosition = commandLength;
    }

    void AddToCommandHistory( const char * string )
    {
        // If the previous command is identical to the new one, don't add to history. It's annoying.
        int previousIndex = commandHistoryIndex - 1;
        if ( previousIndex < 0 )
            previousIndex += CommandHistorySize;
        if ( strcmp( string, &commandHistory[previousIndex][0] ) == 0 )
            return;

        strncpy_safe( &commandHistory[commandHistoryIndex][0], string, MaxLine );
        commandHistoryIndex++;
        commandHistoryIndex %= CommandHistorySize;
        commandHistoryIsEmpty = false;
    }

    void NextCommandInHistory()
    {
        if ( commandHistoryIsEmpty )
            return;

        if ( commandHistorySelection == -1 )
        {
            commandHistorySelection = commandHistoryIndex;
            AddToCommandHistory( commandString );
        }

        for ( int i = 0; i < CommandHistorySize; ++i )
        {
            commandHistorySelection++;
            commandHistorySelection %= CommandHistorySize;

            if ( commandHistory[commandHistorySelection][0] != '\0' )
            {
                const char * command = &commandHistory[commandHistorySelection][0];
                memset( commandString, 0, sizeof( commandString ) );
                strncpy_safe( commandString, command, MaxLine );
                commandLength = strlen( commandString );
                CursorEnd();
                return;
            }
        }
    }

    void PrevCommandInHistory()
    {
        if ( commandHistoryIsEmpty )
            return;

        if ( commandHistorySelection == -1 )
        {
            commandHistorySelection = commandHistoryIndex;
            AddToCommandHistory( commandString );
        }

        for ( int i = 0; i < CommandHistorySize; ++i )
        {
            commandHistorySelection--;
            if ( commandHistorySelection < 0 )
                commandHistorySelection += CommandHistorySize;

            if ( commandHistory[commandHistorySelection][0] != '\0' )
            {
                const char * command = &commandHistory[commandHistorySelection][0];
                memset( commandString, 0, sizeof( commandString ) );
                strncpy_safe( commandString, command, MaxLine );
                commandLength = strlen( commandString );
                CursorEnd();
                return;
            }
        }
    }
};

Console::Console( core::Allocator & allocator )
{
    m_allocator = &allocator;
    m_internal = CORE_NEW( allocator, ConsoleInternal );
}

Console::~Console()
{
    CORE_DELETE( *m_allocator, ConsoleInternal, m_internal );
}

bool Console::KeyEvent( int key, int scancode, int action, int mods )
{
    if ( !IsActive() )
    {
        if ( key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS && mods == 0 )
        {
            m_internal->Activate( true );
            return true;
        }

        return false;
    }
    else
    {
        if ( ( key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER ) && action == GLFW_PRESS && mods == 0 )
        {
            ExecuteCommand( m_internal->commandString );
            m_internal->AddToCommandHistory( m_internal->commandString );
            m_internal->ClearCommandString();
            m_internal->Deactivate( false );
        }
        else if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && mods == 0 )
        {
            m_internal->ClearCommandString();
        }
        else if ( key == GLFW_KEY_BACKSPACE && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == 0 )
        {
            m_internal->Backspace();
        }
        else if ( key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS && mods == 0 )
        {
            m_internal->ClearCommandString();
            m_internal->Deactivate( true );
        }
        else if ( key == GLFW_KEY_UP && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == 0 )
        {
            m_internal->PrevCommandInHistory();
        }
        else if ( key == GLFW_KEY_DOWN && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == 0 )
        {
            m_internal->NextCommandInHistory();
        }
        else if ( key == GLFW_KEY_LEFT && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == 0 )
        {
            m_internal->CursorLeft();
        }
        else if ( key == GLFW_KEY_RIGHT && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == 0 )
        {
            m_internal->CursorRight();
        }
        else if ( key == GLFW_KEY_LEFT && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == GLFW_MOD_SUPER )
        {
            m_internal->CursorBegin();
        }
        else if ( key == GLFW_KEY_RIGHT && ( action == GLFW_PRESS || action == GLFW_REPEAT ) && mods == GLFW_MOD_SUPER )
        {
            m_internal->CursorEnd();
        }
    }

    return true;
}

bool Console::CharEvent( unsigned int code )
{
    if ( m_internal->justDeactivated )
    {
        m_internal->justDeactivated = false;
        if ( code == '`' )
            return true;
    }

    if ( !IsActive() )
        return false;

    if ( code >= 255 )
        return false;

    if ( m_internal->justActivated )
    {
        m_internal->justActivated = false;
        if ( code == '`' )
            return true;
    }

    m_internal->CharacterTyped( (char) code );

    return true;
}

void Console::ExecuteCommand( const char * string )
{
    if ( strcmp( string, "quit" ) == 0 || strcmp( string, "exit" ) == 0 )
        global.quit = true;

    // ...
}

bool Console::IsActive() const
{
    return m_internal->active;
}

const int MaxConsoleVertices = 1024;

struct ConsoleVertex
{
    float x,y,z;
    float r,g,b,a;
};

static void InitRender( ConsoleInternal & internal )
{
    glGenVertexArrays( 1, &internal.vao );
    glBindVertexArray( internal.vao );

    glEnableVertexAttribArray( 0 );
    glEnableVertexAttribArray( 1 );

    glGenBuffers( 1, &internal.vbo );
    glBindBuffer( GL_ARRAY_BUFFER, internal.vbo );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof(ConsoleVertex), (GLubyte*)0 );
    glVertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, sizeof(ConsoleVertex), (GLubyte*)(3*4) );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
}

static void RenderTriangles( ConsoleInternal & internal, const ConsoleVertex * vertices, int numTriangles )
{
    CORE_ASSERT( numTriangles > 0 );
    CORE_ASSERT( numTriangles * 3 <= MaxConsoleVertices );

    GLuint shader_program = global.shaderManager->GetShader( "Console" );
    if ( !shader_program )
        return;

    glUseProgram( shader_program );

    mat4 modelViewProjection = glm::ortho( 0.0f, (float) global.displayWidth, (float) global.displayHeight, 0.0f, -1.0f, 1.0f );
    int location = glGetUniformLocation( shader_program, "ModelViewProjection" );
    if ( location < 0 )
        return;

    glUniformMatrix4fv( location, 1, GL_FALSE, &modelViewProjection[0][0] );

    glEnable( GL_BLEND );

    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glBindVertexArray( internal.vao );

    glBindBuffer( GL_ARRAY_BUFFER, internal.vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof( ConsoleVertex ) * numTriangles * 3, vertices, GL_STREAM_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    glDrawArrays( GL_TRIANGLES, 0, numTriangles * 3 );

    glBindVertexArray( 0 );

    glUseProgram( 0 );
}

static void RenderQuad( ConsoleInternal & internal, const ConsoleVertex & a, const ConsoleVertex & b, const ConsoleVertex & c, const ConsoleVertex & d )
{
    ConsoleVertex vertex[6];

    vertex[0] = a;
    vertex[1] = b;
    vertex[2] = c;
    vertex[3] = a;
    vertex[4] = c;
    vertex[5] = d;

    RenderTriangles( internal, vertex, 2 );
}

static void RenderBackground( ConsoleInternal & internal )
{
    // ...
}

static void RenderCursor( ConsoleInternal & internal, float x, float y, float width, float height )
{
    ConsoleVertex a,b,c,d;

    a.x = x;
    a.y = y;
    a.z = 0;
    a.r = 0;
    a.g = 0;
    a.b = 0;
    a.a = 0.5f;

    b.x = x + width;
    b.y = y;
    b.z = 0;
    b.r = 0;
    b.g = 0;
    b.b = 0;
    b.a = 0.5f;

    c.x = x + width;
    c.y = y + height;
    c.z = 0;
    c.r = 0;
    c.g = 0;
    c.b = 0;
    c.a = 0.5f;

    d.x = x;
    d.y = y + height;
    d.z = 0;
    d.r = 0;
    d.g = 0;
    d.b = 0;
    d.a = 0.5f;

    RenderQuad( internal, a, b, c, d );
}

void Console::Render()
{
    if ( !m_internal->active )
        return;

    Font * font = global.fontManager->GetFont( "Console" );
    if ( !font )
        return;

    if ( !m_internal->vao )
        InitRender( *m_internal );

    RenderBackground( *m_internal );

    const float command_origin_x = 3;
    const float command_origin_y = 3;

    if ( m_internal->commandString[0] != '\0' )
    {
        font->Begin();
        font->DrawText( command_origin_x, command_origin_y, m_internal->commandString, Color(0,0,0,1.0) );
        font->End();
    }

    const float font_width = font->GetCharWidth( 'a' );
    const float font_height = font->GetLineHeight();

    RenderCursor( *m_internal, command_origin_x + m_internal->commandCursorPosition * font_width, command_origin_y, font_width, font_height );
}