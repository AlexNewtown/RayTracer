#include <limits>
#include "RayTracer.h"
#include "Image.h"
#include "Object.h"
#include "Sphere.h"
#include "Intersection.h"
#include "Light.h"

using namespace std;

RayTracer::RayTracer(int width_, int height_, int maxReflections_, int superSamples_,
 int depthComplexity_) : width(width_), height(height_),
 maxReflections(maxReflections_), superSamples(superSamples_),
 depthComplexity(depthComplexity_) {
   cameraPosition = Vector(0.0, 0.0, 100.0);
   focalPointLength = 100.0;
   dispersion = 5.0f;
}

RayTracer::~RayTracer() {
   for (vector<Object*>::iterator itr = objects.begin(); itr < objects.end(); itr++) {
      delete *itr;
   }

   for (vector<Light*>::iterator itr = lights.begin(); itr < lights.end(); itr++) {
      delete *itr;
   }
}

void RayTracer::traceRays(string fileName) {
   Image image(width, height);

   for (int x = 0; x < width; x++) {
      // Update percent complete.
      float percentage = x/(float)width * 100;
      cout << '\r' << (int)percentage << '%';
      fflush(stdout);

      for (int y = 0; y < height; y++) {
         image.pixel(x, y, castRayForPixel(x, y));
      }
   }

   cout << "\rDone!" << endl;

   image.WriteTga(fileName.c_str(), true);
}

Color RayTracer::castRayForPixel(int x, int y) {
   /**
    * All of these values are "on the focal plane." That is to say rays will
    * be cast from the camera position towards points created from these
    * values.
    */
   double rayX = (x - width / 2)/2.0;
   double rayY = (y - height / 2)/2.0;
   double pixelWidth = rayX - (x + 1 - width / 2)/2.0;
   double sampleWidth = pixelWidth / superSamples;
   double sampleStartX = rayX - pixelWidth/2.0;
   double sampleStartY = rayY - pixelWidth/2.0;
   double sampleWeight = 1.0 / (superSamples * superSamples);
   Color color;

   for (int x = 0; x < superSamples; x++) {
      for (int y = 0; y < superSamples; y++) {
         Vector imagePlanePoint = Vector(sampleStartX + (x * sampleWidth),
          sampleStartY + (y * sampleWidth), 0);

         color = color + (castRayWithXY(imagePlanePoint) * sampleWeight);
      }
   }

   return color;
}

Color RayTracer::castRayWithXY(Vector direction) {
   Color color;
   // Divide by projection distance.
   direction.x /= 500.0;
   direction.y /= 500.0;
   direction.z = -1;

   direction = direction.normalize();
   Vector aimed = cameraPosition + (direction * focalPointLength);

   for (int i = 0; i < depthComplexity; i++) {
      Ray viewRay(cameraPosition, direction, maxReflections);

      if (depthComplexity > 1) {
         Vector disturbance(
          (dispersion / RAND_MAX) * (1.0f * rand()),
          (dispersion / RAND_MAX) * (1.0f * rand()),
          0.0f);

         viewRay.origin = viewRay.origin + disturbance;
         viewRay.direction = aimed - viewRay.origin;
         viewRay.direction = viewRay.direction.normalize();
      }

      color = color + (castRay(viewRay) * (1 / (float)depthComplexity));
   }

   return color;
}

Color RayTracer::castRay(Ray ray) {
   Intersection intersection = getClosestIntersection(ray);

   if (intersection.didIntersect) {
      return performLighting(intersection);
   } else {
      return Color();
   }
}

Intersection RayTracer::getClosestIntersection(Ray ray) {
   Intersection closestIntersection(false);
   closestIntersection.distance = numeric_limits<double>::max();

   for (vector<Object*>::iterator itr = objects.begin(); itr < objects.end(); itr++) {
      Intersection intersection = (*itr)->intersect(ray);

      if (intersection.didIntersect && intersection.distance <
       closestIntersection.distance) {
         closestIntersection = intersection;
      }
   }

   return closestIntersection;
}

Color RayTracer::performLighting(Intersection intersection) {
   Color ambientColor = getAmbientLighting(intersection);
   Color diffuseAndSpecularColor = getDiffuseAndSpecularLighting(intersection);
   Color reflectedColor = getReflectiveLighting(intersection);

   return ambientColor + diffuseAndSpecularColor + reflectedColor;
}

Color RayTracer::getAmbientLighting(Intersection intersection) {
   return intersection.color * 0.2;
}

Color RayTracer::getDiffuseAndSpecularLighting(Intersection intersection) {
   Color diffuseColor(0.0, 0.0, 0.0);
   Color specularColor(0.0, 0.0, 0.0);

   for (vector<Light*>::iterator itr = lights.begin(); itr < lights.end(); itr++) {
      Light* light = *itr;
      Vector lightOffset = light->position - intersection.intersection;
      /**
       * TODO: Be careful about normalizing lightOffset too.
       */
      Vector lightDirection = lightOffset.normalize();
      double dotProduct = intersection.normal.dot(lightDirection);

      /**
       * Intersection is facing light.
       */
      if (dotProduct >= 0.0f) {
         Ray shadowRay = Ray(intersection.intersection, lightDirection, 1);
         Intersection shadowIntersection = getClosestIntersection(shadowRay);

         if (shadowIntersection.didIntersect) {
            /**
             * Position is in shadow of another object - continue with other lights.
             */
            continue;
         }

         diffuseColor = diffuseColor + (intersection.color * dotProduct);
         specularColor = specularColor + getSpecularLighting(intersection, light);
      }
   }

   return diffuseColor + specularColor;
}

Color RayTracer::getSpecularLighting(Intersection intersection, Light* light) {
   Color specularColor(0.0, 0.0, 0.0);
   double shininess = intersection.object->getShininess();

   if (shininess == NOT_SHINY) {
      /* Don't perform specular lighting on non shiny objects. */
      return specularColor;
   }

   Vector view = (intersection.ray.origin - intersection.intersection).normalize();
   Vector lightOffset = light->position - intersection.intersection;
   Vector reflected = reflectVector(lightOffset.normalize(), intersection.normal);

   double dot = view.dot(reflected);

   if (dot <= 0) {
      return specularColor;
   }

   double specularAmount = pow(dot, shininess);

   specularColor.r = specularAmount;
   specularColor.g = specularAmount;
   specularColor.b = specularAmount;

   return specularColor;
}

Color RayTracer::getReflectiveLighting(Intersection intersection) {
   double reflectivity = intersection.object->getReflectivity();
   int reflectionsRemaining = intersection.ray.reflectionsRemaining;

   if (reflectivity == NOT_REFLECTIVE || reflectionsRemaining <= 0) {
      return Color();
   } else {
      Vector reflected = reflectVector(intersection.ray.origin, intersection.normal);
      Ray reflectedRay(intersection.intersection, reflected, reflectionsRemaining - 1);

      return castRay(reflectedRay) * reflectivity;
   }
}

Vector RayTracer::reflectVector(Vector vector, Vector normal) {
   return normal * 2 * vector.dot(normal) - vector;
}

void RayTracer::readScene(istream& in) {
   string type;

   in >> type;

   while (in.good()) {
      if (type.compare("sphere") == 0) {
         Vector center;
         double radius;
         Color color;
         double shininess;
         double reflectivity;

         in >> center.x >> center.y >> center.z;
         in >> radius;
         in >> color.r >> color.g >> color.b;
         in >> shininess;
         in >> reflectivity;

         addObject(new Sphere(center, radius, color, shininess, reflectivity));
      } else if (type.compare("light") == 0) {
         Vector position;

         in >> position.x >> position.y >> position.z;

         addLight(new Light(position));
      } else {
         cerr << "Type not found: " << type << endl;
         exit(EXIT_FAILURE);
      }

      in >> type;
   }
}

