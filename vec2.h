#ifndef VEC2_H
#define VEC2_H


struct vec2 {
    float x, y;
    vec2(float x, float y) : x(x), y(y) {}
    vec2() : x(0), y(0) {}
};


vec2 operator + (vec2 a, vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

vec2 operator - (vec2 a, vec2 b) {
    return vec2(a.x - b.x, a.y - b.y);
}

vec2 operator * (float s, vec2 a) {
    return vec2(s * a.x, s * a.y);
}


vec2 getBezierPoint( vec2* points, int numPoints, float t ) { 
    vec2* tmp = new vec2[numPoints];
    memcpy(tmp, points, numPoints * sizeof(vec2));
    int i = numPoints - 1;
    while (i > 0) {
        for (int k = 0; k < i; k++)
            tmp[k] = tmp[k] + t * ( tmp[k+1] - tmp[k] );
        i--;
    }   
    vec2 answer = tmp[0];
    delete[] tmp;
    return answer;
}




#endif // VEC2_H
