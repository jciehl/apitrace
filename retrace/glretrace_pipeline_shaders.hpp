
#ifndef _GLRETRACE_PIPELINE_SHADERS_H_
#define _GLRETRACE_PIPELINE_SHADERS_H_

#include <cmath>
#include <limits>


namespace glretrace {
namespace pipelineview {

//! create a shader object.
GLuint createShader( const GLenum type );
//! create and compile a shader object.
GLuint createShader( const GLenum type, const char *source );
//! cleanup created shader objects.
void cleanupShaders( );

//! create a shader program object.
GLuint createProgram( );
GLuint createProgram( const GLuint vertex, const GLuint fragment );
GLuint createProgram( const char *vertex, const char *fragment );
bool linkProgram( const GLuint program );
void cleanupPrograms( );

//! assign uniforms
bool assignProgramUniforms( const GLint program, const GLint activeProgram );


//! basic Vec3 class.
struct Vec3 {
    float x, y, z;
    
    Vec3( ) : x(0.f), y(0.f), z(0.f) {}
    Vec3( const float v ) : x(v), y(v), z(v) {}
    Vec3( const float _x, const float _y, const float _z ) : x(_x), y(_y), z(_z) {}
    Vec3( const Vec3& b ) : x(b.x), y(b.y), z(b.z) {}
    Vec3& operator= ( const Vec3& b ) {
        x= b.x;
        y= b.y;
        z= b.z;
        return *this;
    }
    
    Vec3& operator+ ( const Vec3& b ) {
        x+= b.x;
        y+= b.y;
        z+= b.z;
        return *this;
    }
    Vec3 operator+ ( const Vec3& b ) const {
        return Vec3(x + b.x, y + b.y, z + b.z);
    }
    Vec3& operator- ( const Vec3& b ) {
        x-= b.x;
        y-= b.y;
        z-= b.z;
        return *this;
    }
    Vec3 operator- ( const Vec3& b ) const {
        return Vec3(x - b.x, y - b.y, z - b.z);
    }
    
    Vec3& operator* ( const float v ) {
        x*= v;
        y*= v;
        z*= v;
        return *this;
    }
    Vec3 operator* ( const Vec3& b ) const {
        return Vec3(x * b.x, y * b.y, z * b.z);
    }
};

typedef Vec3 Point;
typedef Vec3 Vector;

//! scalar product.
static inline 
float Dot( const Vector &v1, const Vector &v2 ) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

//! cross product.
static inline 
Vector Cross( const Vector &v1, const Vector &v2 ) {
    return Vector(
        ( v1.y * v2.z ) - ( v1.z * v2.y ),
        ( v1.z * v2.x ) - ( v1.x * v2.z ),
        ( v1.x * v2.y ) - ( v1.y * v2.x ) );
}

//! distance from point a to point b.
static inline
float Distance( const Point& a, const Point& b ) {
    Vector v(b - a);
    return sqrtf(Dot(v, v));
}

//! unit length direction from vector.
static inline 
Vector Normalize( const Vector &v )
{
    const float inv_length= 1.f / sqrtf(Dot(v, v));
    return v * inv_length;
}

//! basic transform / matrix class.
struct Transform {
    float m[4][4];
    
    Transform( ) {
        for(int r= 0; r < 4; r++) {
            for(int c= 0; c < 4; c++) {
                m[r][c]= Id[r][c];
            }
        }
    }
    Transform( const float _m[4][4] ) {
        for(int r= 0; r < 4; r++) {
            for(int c= 0; c < 4; c++) {
                m[r][c]= _m[r][c];
            }
        }
    }
    
    Transform(
        const float t00, const float t01, const float t02, const float t03,
        const float t10, const float t11, const float t12, const float t13,
        const float t20, const float t21, const float t22, const float t23,
        const float t30, const float t31, const float t32, const float t33 )
    {
        m[0][0]= t00; m[0][1]= t01; m[0][2]= t02; m[0][3]= t03;
        m[1][0]= t10; m[1][1]= t11; m[1][2]= t12; m[1][3]= t13;
        m[2][0]= t20; m[2][1]= t21; m[2][2]= t22; m[2][3]= t23;
        m[3][0]= t30; m[3][1]= t31; m[3][2]= t32; m[3][3]= t33;
    }
    
    Transform( const Transform& b ) {
        Transform(b.m);
    }
    
    float *matrix( ) {
        return &m[0][0];
    }
    
    Transform operator* ( const Transform& b ) const {
        float t[4][4];
        for (int r= 0; r < 4; r++) {
            for (int c= 0; c < 4; c++) {
                t[r][c] = 
                    m[r][0] * b.m[0][c] +
                    m[r][1] * b.m[1][c] +
                    m[r][2] * b.m[2][c] +
                    m[r][3] * b.m[3][c];
            }
        }
        return Transform(t);
    }
    
    //~ //! affiche la matrice.
    //~ void print( ) const
    //~ {
        //~ #define M44(m, r, c) m[r][c]
        
        //~ os::log("% -.8f  % -.8f  % -.8f  % -.8f\n", 
            //~ M44(m, 0, 0), M44(m, 0, 1), M44(m, 0, 2), M44(m, 0, 3));
        //~ os::log("% -.8f  % -.8f  % -.8f  % -.8f\n", 
            //~ M44(m, 1, 0), M44(m, 1, 1), M44(m, 1, 2), M44(m, 1, 3));
        //~ os::log("% -.8f  % -.8f  % -.8f  % -.8f\n", 
            //~ M44(m, 2, 0), M44(m, 2, 1), M44(m, 2, 2), M44(m, 2, 3));
        //~ os::log("% -.8f  % -.8f  % -.8f  % -.8f\n", 
            //~ M44(m, 3, 0), M44(m, 3, 1), M44(m, 3, 2), M44(m, 3, 3));
        //~ os::log("\n");
        
        //~ #undef M44
    //~ }    
    
    static const float Id[4][4];
};

//! build an openGL perspective transform.
static inline
Transform Perspective( const float fov, const float aspect, const float znear, const float zfar ) {
    const float inv_tan = 1.f / tanf( fov / 2.f / 180.f * (float) M_PI );
    const float inv_denom = 1.f / ( znear - zfar );
    Transform persp( 
        inv_tan/aspect,       0,                    0,                      0,
                     0, inv_tan,                    0,                      0,
                     0,       0, (zfar+znear)*inv_denom, 2.f*zfar*znear*inv_denom,
                     0,       0,                   -1,                      0
    );

    return persp;
}

//! build a look at transform.
static inline
Transform LookAt( const Point &pos, const Point &look, const Vector &up )
{
    // translation part
    Transform inv_r;
    Vector dir = Normalize( Vector(look - pos) );
    Vector right = Normalize( Cross(dir, Normalize(up)) );
    Vector newUp = Normalize( Cross(right, dir) );
    inv_r.m[0][0] = right.x;
    inv_r.m[0][1] = right.y;
    inv_r.m[0][2] = right.z;
    inv_r.m[0][3] = 0.f;
    inv_r.m[1][0] = newUp.x;
    inv_r.m[1][1] = newUp.y;
    inv_r.m[1][2] = newUp.z;
    inv_r.m[1][3] = 0.f;
    inv_r.m[2][0] = -dir.x;   // opengl convention, look down the negative z axis
    inv_r.m[2][1] = -dir.y;
    inv_r.m[2][2] = -dir.z;
    inv_r.m[2][3] = 0.f;

    // rotation part
    Transform inv_t;
    inv_t.m[0][3] = -pos.x;
    inv_t.m[1][3] = -pos.y;
    inv_t.m[2][3] = -pos.z;
    inv_t.m[3][3] = 1.f;

    return inv_r*inv_t;
}

}       // namespace pipeline view
}       // namespace glretrace

#endif
