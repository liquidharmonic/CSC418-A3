/***********************************************************
     Starter code for Assignment 3

     This code was originally written by Jack Wang for
		    CSC418, SPRING 2005

		Implementations of functions in raytracer.h, 
		and the main function which specifies the 
		scene to be rendered.	

***********************************************************/


#include "raytracer.h"
#include "bmp_io.h"
#include <cmath>
#include <iostream>
#include <cstdlib>

Raytracer::Raytracer() : _lightSource(NULL) {
	_root = new SceneDagNode();
}

Raytracer::~Raytracer() {
	delete _root;
}

SceneDagNode* Raytracer::addObject( SceneDagNode* parent, 
		SceneObject* obj, Material* mat ) {
	SceneDagNode* node = new SceneDagNode( obj, mat );
	node->parent = parent;
	node->next = NULL;
	node->child = NULL;
	
	// Add the object to the parent's child list, this means
	// whatever transformation applied to the parent will also
	// be applied to the child.
	if (parent->child == NULL) {
		parent->child = node;
	}
	else {
		parent = parent->child;
		while (parent->next != NULL) {
			parent = parent->next;
		}
		parent->next = node;
	}
	
	return node;;
}

LightListNode* Raytracer::addLightSource( LightSource* light ) {
	LightListNode* tmp = _lightSource;
	_lightSource = new LightListNode( light, tmp );
	return _lightSource;
}

void Raytracer::rotate( SceneDagNode* node, char axis, double angle ) {
	Matrix4x4 rotation;
	double toRadian = 2*M_PI/360.0;
	int i;
	
	for (i = 0; i < 2; i++) {
		switch(axis) {
			case 'x':
				rotation[0][0] = 1;
				rotation[1][1] = cos(angle*toRadian);
				rotation[1][2] = -sin(angle*toRadian);
				rotation[2][1] = sin(angle*toRadian);
				rotation[2][2] = cos(angle*toRadian);
				rotation[3][3] = 1;
			break;
			case 'y':
				rotation[0][0] = cos(angle*toRadian);
				rotation[0][2] = sin(angle*toRadian);
				rotation[1][1] = 1;
				rotation[2][0] = -sin(angle*toRadian);
				rotation[2][2] = cos(angle*toRadian);
				rotation[3][3] = 1;
			break;
			case 'z':
				rotation[0][0] = cos(angle*toRadian);
				rotation[0][1] = -sin(angle*toRadian);
				rotation[1][0] = sin(angle*toRadian);
				rotation[1][1] = cos(angle*toRadian);
				rotation[2][2] = 1;
				rotation[3][3] = 1;
			break;
		}
		if (i == 0) {
		    node->trans = node->trans*rotation; 	
			angle = -angle;
		} 
		else {
			node->invtrans = rotation*node->invtrans; 
		}	
	}
}

void Raytracer::translate( SceneDagNode* node, Vector3D trans ) {
	Matrix4x4 translation;
	
	translation[0][3] = trans[0];
	translation[1][3] = trans[1];
	translation[2][3] = trans[2];
	node->trans = node->trans*translation; 	
	translation[0][3] = -trans[0];
	translation[1][3] = -trans[1];
	translation[2][3] = -trans[2];
	node->invtrans = translation*node->invtrans; 
}

void Raytracer::scale( SceneDagNode* node, Point3D origin, double factor[3] ) {
	Matrix4x4 scale;
	
	scale[0][0] = factor[0];
	scale[0][3] = origin[0] - factor[0] * origin[0];
	scale[1][1] = factor[1];
	scale[1][3] = origin[1] - factor[1] * origin[1];
	scale[2][2] = factor[2];
	scale[2][3] = origin[2] - factor[2] * origin[2];
	node->trans = node->trans*scale; 	
	scale[0][0] = 1/factor[0];
	scale[0][3] = origin[0] - 1/factor[0] * origin[0];
	scale[1][1] = 1/factor[1];
	scale[1][3] = origin[1] - 1/factor[1] * origin[1];
	scale[2][2] = 1/factor[2];
	scale[2][3] = origin[2] - 1/factor[2] * origin[2];
	node->invtrans = scale*node->invtrans; 
}

Matrix4x4 Raytracer::initInvViewMatrix( Point3D eye, Vector3D view, 
		Vector3D up ) {
	Matrix4x4 mat; 
	Vector3D w;
	view.normalize();
	up = up - up.dot(view)*view;
	up.normalize();
	w = view.cross(up);

	mat[0][0] = w[0];
	mat[1][0] = w[1];
	mat[2][0] = w[2];
	mat[0][1] = up[0];
	mat[1][1] = up[1];
	mat[2][1] = up[2];
	mat[0][2] = -view[0];
	mat[1][2] = -view[1];
	mat[2][2] = -view[2];
	mat[0][3] = eye[0];
	mat[1][3] = eye[1];
	mat[2][3] = eye[2];

	return mat; 
}

void Raytracer::traverseScene( SceneDagNode* node, Ray3D& ray ) {
	SceneDagNode *childPtr;

	// Applies transformation of the current node to the global
	// transformation matrices.
	_modelToWorld = _modelToWorld*node->trans;
	_worldToModel = node->invtrans*_worldToModel; 
	if (node->obj) {
		// Perform intersection.
		if (node->obj->intersect(ray, _worldToModel, _modelToWorld)) {
			ray.intersection.mat = node->mat;
		}
	}
	// Traverse the children.
	childPtr = node->child;
	while (childPtr != NULL) {
		traverseScene(childPtr, ray);
		childPtr = childPtr->next;
	}

	// Removes transformation of the current node from the global
	// transformation matrices.
	_worldToModel = node->trans*_worldToModel;
	_modelToWorld = _modelToWorld*node->invtrans;
}

Colour Raytracer::helpShade(Ray3D& ray, LightListNode* curLight, int n, float k)
{  
	Vector3D shadowDir;
	shadowDir = curLight->light->get_position() - ray.intersection.point;
	shadowDir[0] += k;
    shadowDir[1] += k;
    shadowDir[2] += k;
	shadowDir.normalize();
	Point3D shadowOrigin = ray.intersection.point + 0.01*shadowDir;

	Ray3D shadowRay(shadowOrigin , shadowDir);
	traverseScene(_root, shadowRay);
	
	// Compute non-shadow colour
	curLight->light->shade(ray);
	
	// If ray intersects another object  it falls in a shadow
	if (!shadowRay.intersection.none) 
		return (1/n)*ray.col;
	else{
		return Colour(0,0,0);
	}
}

void Raytracer::computeShading( Ray3D& ray ) {
	LightListNode* curLight = _lightSource;
	for (;;) {
		if (curLight == NULL) break;
		// Each lightSource provides its own shading function.

		// Implement shadows here if needed.

        // Compute if shadow should appear
        // Create new ray from intersection point to light source
       
        //curLight = curLight->next;
        Colour tmp;
        for (float i = 0 ; i<2.5;i = i + 0.50){
			tmp = helpShade(ray,curLight, 5,i);
			ray.col[0] += tmp[0];
			ray.col[1] += tmp[1];
			ray.col[2] += tmp[2];
			/*
			 * _rbuffer[i*width+j] += int(col[0]*255*coef);
				_gbuffer[i*width+j] += int(col[1]*255*coef);
				_bbuffer[i*width+j] += int(col[2]*255*coef);
					*/
		}
		curLight = curLight->next;
	
	}
}



void Raytracer::initPixelBuffer() {
	int numbytes = _scrWidth * _scrHeight * sizeof(unsigned char);
	_rbuffer = new unsigned char[numbytes];
	_gbuffer = new unsigned char[numbytes];
	_bbuffer = new unsigned char[numbytes];
	for (int i = 0; i < _scrHeight; i++) {
		for (int j = 0; j < _scrWidth; j++) {
			_rbuffer[i*_scrWidth+j] = 0;
			_gbuffer[i*_scrWidth+j] = 0;
			_bbuffer[i*_scrWidth+j] = 0;
		}
	}
}

void Raytracer::flushPixelBuffer( char *file_name ) {
	bmp_write( file_name, _scrWidth, _scrHeight, _rbuffer, _gbuffer, _bbuffer );
	delete _rbuffer;
	delete _gbuffer;
	delete _bbuffer;
}

Colour Raytracer::shadeRay( Ray3D& ray ) {
	Colour col(0.0, 0.0, 0.0); 

	traverseScene(_root, ray); 
	
	// Don't bother shading if the ray didn't hit 
	// anything.
	if (!ray.intersection.none) {
		computeShading(ray);

        // You'll want to call shadeRay recursively (with a different ray, 
    	// of course) here to implement reflection/refraction effects.  
        float dampFactor = 0.0;

        // Calculate reflection ray
        Vector3D N = ray.intersection.normal;
        Vector3D D = ray.dir;
        Vector3D reflectionDir = -2*(D.dot(N))*N + D;
        reflectionDir.normalize();
        Point3D reflectionOrigin = ray.intersection.point + 0.01*reflectionDir;
        Ray3D reflectionRay = Ray3D(reflectionOrigin, reflectionDir);

        // calculate shade of reflected ray
        shadeRay(reflectionRay);

        // limit effective distance of reflections
        if (reflectionRay.intersection.t_value > 10.0 || reflectionRay.intersection.t_value <= 0.0) {
            col = ray.col;
        }
        else {
        dampFactor = fabs(1.0/reflectionRay.intersection.t_value);
        // contraint dampFactor to 0-0.9
        dampFactor = fmax(0, fmin(dampFactor,0.9));
        // Set colour to include reflection
        col = ray.col + dampFactor*reflectionRay.col;
        }

        col.clamp();
	}
	return col; 
}	

void Raytracer::render( int width, int height, Point3D eye, Vector3D view, 
		Vector3D up, double fov, char* fileName) {
	Matrix4x4 viewToWorld;
	_scrWidth = width;
	_scrHeight = height;
	double factor = (double(height)/2)/tan(fov*M_PI/360.0);

	initPixelBuffer();
	viewToWorld = initInvViewMatrix(eye, view, up);

	// Construct a ray for each pixel.
	for (int i = 0; i < _scrHeight; i++) {
		for (int j = 0; j < _scrWidth; j++) {
			//perform antialiasing with a factor of 4 so ray is computed 4 times at a pixel
			//and multiplied by factor 1/4 and summed together for antialiased image
			 for(float fragmenti = i; fragmenti < i + 1.0f; fragmenti += 0.5f){
				for(float fragmentj = j; fragmentj < j + 1.0f; fragmentj += 0.5f){
					// Sets up ray origin and direction in view space, 
					// image plane is at z = -1.
					Point3D origin(0, 0, 0);
					Point3D imagePlane;
					imagePlane[0] = (-double(width)/2 + 0.5 + fragmentj)/factor;
					imagePlane[1] = (-double(height)/2 + 0.5 + fragmenti)/factor;
					imagePlane[2] = -1;

					// TODO: Convert ray to world space and call 
					// shadeRay(ray) to generate pixel colour. 
					//multiply each ray by 1/4	
					float coef = 0.25f; 
					Ray3D ray;
					// want ray to be in world frame
					ray.origin = viewToWorld*origin;
					ray.dir = viewToWorld*(imagePlane-origin);
					ray.dir.normalize();

					Colour col = shadeRay(ray); 
					//now sum the total contributions from 4 rays per pixel
					_rbuffer[i*width+j] += int(col[0]*255*coef);
					_gbuffer[i*width+j] += int(col[1]*255*coef);
					_bbuffer[i*width+j] += int(col[2]*255*coef);
				}
			}
		}
	}

	flushPixelBuffer(fileName);
}

int main(int argc, char* argv[])
{	
	// Build your scene and setup your camera here, by calling 
	// functions from Raytracer.  The code here sets up an example
	// scene and renders it from two different view points, DO NOT
	// change this if you're just implementing part one of the 
	// assignment.  
	Raytracer raytracer;
	int width = 320; 
	int height = 240; 

	if (argc == 3) {
		width = atoi(argv[1]);
		height = atoi(argv[2]);
	}

	// Camera parameters.
	Point3D eye(0, 0, 1);
	Vector3D view(0, 0, -1);
	Vector3D up(0, 1, 0);
	double fov = 60;

	// Defines a material for shading.
	Material gold( Colour(0.3, 0.3, 0.3), Colour(0.75164, 0.60648, 0.22648), 
			Colour(0.628281, 0.555802, 0.366065), 
			51.2 );
	Material jade( Colour(0, 0, 0), Colour(0.54, 0.89, 0.63), 
			Colour(0.316228, 0.316228, 0.316228), 
			12.8 );

	// Defines a point light source.
	raytracer.addLightSource( new PointLight(Point3D(0, 0, 5), 
				Colour(0.9, 0.9, 0.9) ) );

	// Add a unit square into the scene with material mat.
	SceneDagNode* sphere = raytracer.addObject( new UnitSphere(), &gold );
	SceneDagNode* plane = raytracer.addObject( new UnitSquare(), &jade );
	//SceneDagNode* sphere = raytracer.addObject( new UnitSphere(), &gold );
	SceneDagNode* cylinder = raytracer.addObject( new UnitCylinder(), &gold );
	// Apply some transformations to the unit square.
	double factor1[3] = { 1.0, 2.0, 1.0 };
	double factor2[3] = { 6.0, 6.0, 6.0 };
	double factor3[3] = { 1.5, 1.5, 1.5 };
	raytracer.translate(sphere, Vector3D(0, 0, -5));	
	raytracer.rotate(sphere, 'x', -45); 
	raytracer.rotate(sphere, 'z', 45); 
	raytracer.scale(sphere, Point3D(0, 0, 0), factor1);

	raytracer.translate(plane, Vector3D(0, 0, -7));	
	raytracer.rotate(plane, 'z', 45); 
	raytracer.scale(plane, Point3D(0, 0, 0), factor2);
	
	raytracer.translate(cylinder, Vector3D(-4, 0, -5));	
	//raytracer.rotate(cylinder, 'z', 45); 
	raytracer.scale(cylinder, Point3D(0, 0, 0), factor3);

	// Render the scene, feel free to make the image smaller for
	// testing purposes.	
	raytracer.render(width, height, eye, view, up, fov, "view1.bmp");
	
	// Render it from a different point of view.
	Point3D eye2(4, 2, 1);
	Vector3D view2(-4, -2, -6);
	raytracer.render(width, height, eye2, view2, up, fov, "view2.bmp");
	
	return 0;
}

