#ifndef __RAY_TRACER_H__
#define __RAY_TRACER_H__

#include <string>
#include <vector>
#include <iostream>
#include "Vector.h"

class Ray;
class Color;
class Intersection;
class Object;
class Light;

class RayTracer {
public:
   int width;
   int height;
   int maxReflections;
   int superSamples; // Square root of number of samples to use for each pixel.
   Vector cameraPosition;
   Vector cameraUp;
   Vector cameraLookAt;
   Vector w, u, v;
   double imageScale;
   int depthComplexity;
   double dispersion;
   unsigned long long raysCast;

   std::vector<Object*> objects;
   std::vector<Light*> lights;

   RayTracer(int, int, int, int, int);

   ~RayTracer();

   void addObject(Object* object) {
      objects.push_back(object);
   }

   void addLight(Light* light) {
      lights.push_back(light);
   }

   void traceRays(std::string);
   Color castRayForPixel(int, int);
   Color castRayAtPoint(Vector);
   Color castRay(Ray);
   Intersection getClosestIntersection(Ray);
   Color performLighting(Intersection);
   Color getAmbientLighting(Intersection);
   Color getDiffuseAndSpecularLighting(Intersection);
   Color getSpecularLighting(Intersection, Light*);
   Color getReflectiveLighting(Intersection);
   Vector reflectVector(Vector, Vector);
   void readScene(std::istream&);
};

#endif
