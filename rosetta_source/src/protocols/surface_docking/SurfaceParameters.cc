// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;
//      rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
//          under license.
// (c) The Rosetta software is developed by the contributing members of the
//          Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions
//          about this can be
// (c) addressed to University of Washington UW TechTransfer,
//                                            email: license@u.washington.edu.

/// @file   SurfaceParameters.hh
/// @brief
/// @author Robin A Thottungal (raugust1@jhu.edu)

// Package Headers
#include <protocols/surface_docking/SurfaceParameters.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <basic/datacache/BasicDataCache.hh>

// Numeric Headers
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <numeric/xyzVector.hh>

// Utility Headers
#include <utility/vector1.hh>
#include <utility/exit.hh>
#include <basic/Tracer.hh>

// C++ headers
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

static basic::Tracer TR("protocols.surfaceDocking.SurfaceParameters");

namespace protocols {
namespace surface_docking {

using namespace numeric;

SurfaceParameters::SurfaceParameters() {
	//These values are used to check for the status of surfaceParameters object
	SURFA0=0;
	SURFA1=0;
	SURFA2=0;

	vecAB=0;
	vecAB=0;
}

SurfaceParameters::SurfaceParameters(std::string strSURFA0,
			std::string strSURFA1, std::string strSURFA2 ) {
	SURFA0=SplitSurfaceVectorString(strSURFA0);
	SURFA1=SplitSurfaceVectorString(strSURFA1);
	SURFA2=SplitSurfaceVectorString(strSURFA2);
	TR<<"Surface Vectors value inside constructor:"<<std::endl;
	TR<<"SURFA0:"<<SURFA0<<std::endl;
	TR<<"SURFA1:"<<SURFA1<<std::endl;
	TR<<"SURFA2:"<<SURFA2<<std::endl;
	vecAB=0;
	vecAB=0;
}

SurfaceParameters::SurfaceParameters( SurfaceParameters const & src ):
	CacheableData() {
	// Setting up the values for caching data

	SURFA0=src.SURFA0;
	SURFA1=src.SURFA1;
	SURFA2=src.SURFA2;

	strSURFA0=src.strSURFA0;
	strSURFA1=src.strSURFA1;
	strSURFA2=src.strSURFA2;
	A=src.A;
	B=src.B;
	C=src.C;
	vecAB=src.vecAB;
	vecAC=src.vecAB;
	SurfaceCG=src.SurfaceCG;
	surfaceNormalVec=src.surfaceNormalVec;
	unitsurfaceNormalVec=src.unitsurfaceNormalVec;
	surfaceAntiNormalVec=src.surfaceAntiNormalVec;
	surfacePlane=src.surfacePlane; // ax+by+cz+d=0
	slideaxis=src.slideaxis;
}

// removed for making class child of CacheableData
//SurfaceParameters::~SurfaceParameters(){}

void SurfaceParameters::GenerateSurfaceParameters( Vector SurfCG ) {
	SurfaceCG=SurfCG;
	// Generating the 2 basis vectors of the surface crystal system
	Vector vecAB0 = SURFA1-SURFA0;
	Vector vecAC0 = SURFA2-SURFA0;
	// Translating the basic vectors to the centroid of the surface
	A=SurfaceCG;
	B=SurfaceCG+vecAB0;
	C=SurfaceCG+vecAC0;
	TR<<"Translated Surface Vectors CoOrdinates"<<std::endl;
	TR<<"SURFA0:"<<A<<std::endl;
	TR<<"SURFA1:"<<B<<std::endl;
	TR<<"SURFA2:"<<C<<std::endl;
	// Generating the 2 basis vectors of the surface crystal system
	// based on new coordinates
	vecAB = B-A;
	vecAC = C-A;
	// Calculate the normal vector to the surface
	surfaceNormalVec=CalcNormalVector(A,B,C);
	unitsurfaceNormalVec=surfaceNormalVec;
	unitsurfaceNormalVec=unitsurfaceNormalVec.normalized();
	TR<<" Surface Normal Vectors:"<<surfaceNormalVec<<std::endl;
	// Calculate the anti-normal vector to the surface
	surfaceAntiNormalVec=-1.0* surfaceNormalVec;
	TR<<" Surface Anti Normal Vectors:"<<surfaceAntiNormalVec<<std::endl;
	// Generate the equation of the plane ax+by+cz+d=0
	surfacePlane = GeneratePlane(A,B,C);

}

Vector SurfaceParameters::CalcAxisIntersect ( Vector point1 , Vector point2 ,
		Vector Bvector , Vector Cvector ){

	core::Real t,DD,w ;
	Vector crossBC,cross1C,CB,CB_C,intersection,inv_crossBC;
	CB = point2 - point1 ;
	cross1C = cross ( CB, Cvector );
	crossBC = cross ( Bvector, Cvector );
	w = inner_product ( cross1C, crossBC);
	DD = inner_product ( crossBC, crossBC);
	t = w / DD;
	return (point1 + t * Bvector); //intersection
}

// Calculate the normal to a plane given three point on the plane
// AB x BC --> normal vector
Vector SurfaceParameters::CalcNormalVector (
				Vector Apoint , Vector Bpoint ,Vector Cpoint ){
	Vector AB,AC;
	AB = Bpoint - Apoint;
	AC = Cpoint - Apoint;
	return cross(AB,AC); //normal

}

// Generate an equation of plane given 3 points that lie on the plane
// ax+by+cz+d=0
Plane SurfaceParameters::GeneratePlane
		( Vector Apoint , Vector Bpoint , Vector Cpoint ){
	Plane plane123;
	Vector normalto3 ;
	normalto3 = CalcNormalVector(  Apoint,  Bpoint, Cpoint );
	plane123.push_back(normalto3.x());
	plane123.push_back(normalto3.y());
	plane123.push_back(normalto3.z());
	plane123.push_back
		(- normalto3.x()* Apoint.x()  -
		normalto3.y()* Apoint.y()  - normalto3.z() * Apoint.z());

	return plane123;
}

Vector SurfaceParameters::PlanePointIntersection( Vector Point ){
	return PlanePointIntersection(surfacePlane,Point,surfaceAntiNormalVec);
}

Vector SurfaceParameters::PlanePointIntersection
	( Plane plane_abcd , Vector point_outofplane , Vector normal_plane ){
	core::Real t;
	Vector point_intersection;
	t=
		-(  plane_abcd [1] * point_outofplane.x() +
			plane_abcd [2] * point_outofplane.y() +
			plane_abcd [3] * point_outofplane.z() + plane_abcd [3] )
		/
		 (  plane_abcd [1] * plane_abcd [1] + plane_abcd [2] * plane_abcd [2]
		  + plane_abcd [3] * plane_abcd [3] );
	point_intersection = point_outofplane - t * normal_plane;
	return point_intersection;
}

Vector SurfaceParameters::SplitSurfaceVectorString( std::string str ){
  //std::istringstream oss (str);
  //std::string word;
  //std::string tmp[4];
  //Size i=0;
  //while(oss >> word) {
  ///	tmp[i]=word;
  ///	i=i+1;
  //	}
  if ( str.size() >= 52 ) {
    utility::vector1< std::string > tmp(3);
    tmp[1] = str.substr( 30, 7 );
    tmp[2] = str.substr( 38, 7 );
    tmp[3] = str.substr( 46, 7 );
    Vector coords( atof( tmp[1].c_str() ), atof( tmp[2].c_str() ), atof( tmp[3].c_str() ));
    return coords;
  } else {
    utility_exit_with_message("Unable to convert remark '" + str + "' into surface vector" );
  }
  return Vector(0.0);  // appease compiler
}

}// namespace surface_docking
}// namespace protocols
