// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file
/// @brief
/// @author

#ifndef INCLUDED_core_scoring_electron_density_util_hh
#define INCLUDED_core_scoring_electron_density_util_hh

#include <core/types.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/conformation/symmetry/SymmetryInfo.fwd.hh>
#include <core/scoring/ScoreType.hh>

#include <numeric/xyzVector.hh>
#include <numeric/fourier/FFT.hh>

#include <ObjexxFCL/FArray3D.hh>
#include <ObjexxFCL/FArray4D.fwd.hh>

#include <complex>
#include <map>

namespace core {
namespace scoring {
namespace electron_density {

// minimal info needed for density scoring
struct poseCoord {
	numeric::xyzVector< core::Real > x_;
	core::Real B_;
	std::string elt_;
};

typedef utility::vector1< poseCoord > poseCoords;

/// @brief update scorefxn with density scores from commandline
void add_dens_scores_from_cmdline_to_scorefxn( core::scoring::ScoreFunction &scorefxn_  );

/// @brief  helper function quickly guesses if a pose has non-zero B factors
bool pose_has_nonzero_Bs( core::pose::Pose const & pose );
bool pose_has_nonzero_Bs( poseCoords const & pose );

/// @brief trilinear interpolation with periodic boundaries
template <class S>
core::Real interp_linear(
	ObjexxFCL::FArray3D< S > const & data ,
	numeric::xyzVector< core::Real > const & idxX) {

	int pt000[3], pt111[3];
	core::Real fpart[3],neg_fpart[3];
	int grid[3];
	grid[0] = data.u1(); grid[1] = data.u2(); grid[2] = data.u3();

	// find bounding grid points
	pt000[0] = (int)(floor(idxX[0])) % grid[0]; if ( pt000[0] <= 0 ) pt000[0]+= grid[0];
	pt000[1] = (int)(floor(idxX[1])) % grid[1]; if ( pt000[1] <= 0 ) pt000[1]+= grid[1];
	pt000[2] = (int)(floor(idxX[2])) % grid[2]; if ( pt000[2] <= 0 ) pt000[2]+= grid[2];
	pt111[0] = (pt000[0]+1); if ( pt111[0]>grid[0] ) pt111[0] = 1;
	pt111[1] = (pt000[1]+1); if ( pt111[1]>grid[1] ) pt111[1] = 1;
	pt111[2] = (pt000[2]+1); if ( pt111[2]>grid[2] ) pt111[2] = 1;

	// interpolation coeffs
	fpart[0] = idxX[0]-floor(idxX[0]); neg_fpart[0] = 1-fpart[0];
	fpart[1] = idxX[1]-floor(idxX[1]); neg_fpart[1] = 1-fpart[1];
	fpart[2] = idxX[2]-floor(idxX[2]); neg_fpart[2] = 1-fpart[2];

	S retval = (S)0.0;
	retval+= neg_fpart[0]*neg_fpart[1]*neg_fpart[2] * data(pt000[0],pt000[1],pt000[2]);
	retval+= neg_fpart[0]*neg_fpart[1]*    fpart[2] * data(pt000[0],pt000[1],pt111[2]);
	retval+= neg_fpart[0]*    fpart[1]*neg_fpart[2] * data(pt000[0],pt111[1],pt000[2]);
	retval+= neg_fpart[0]*    fpart[1]*    fpart[2] * data(pt000[0],pt111[1],pt111[2]);
	retval+= fpart[0]*neg_fpart[1]*neg_fpart[2] * data(pt111[0],pt000[1],pt000[2]);
	retval+= fpart[0]*neg_fpart[1]*    fpart[2] * data(pt111[0],pt000[1],pt111[2]);
	retval+= fpart[0]*    fpart[1]*neg_fpart[2] * data(pt111[0],pt111[1],pt000[2]);
	retval+= fpart[0]*    fpart[1]*    fpart[2] * data(pt111[0],pt111[1],pt111[2]);

	return retval;
}

/// @brief spline interpolation with periodic boundaries
core::Real
interp_spline( ObjexxFCL::FArray3D< core::Real > & coeffs ,
	numeric::xyzVector<core::Real> const & idxX,
	bool mirrored=false
);

numeric::xyzVector<core::Real>
interp_dspline( ObjexxFCL::FArray3D< core::Real > & coeffs ,
	numeric::xyzVector<core::Real> const & idxX,
	bool mirrored=false
);

/// @brief precompute spline coefficients (float array => core::Real coeffs)
void spline_coeffs( ObjexxFCL::FArray3D< core::Real > const &data, ObjexxFCL::FArray3D< core::Real > & coeffs, bool mirrored=false);

/// @brief spline interpolation with periodic boundaries
core::Real
interp_spline( ObjexxFCL::FArray3D< float > & coeffs ,
	numeric::xyzVector<core::Real> const & idxX,
	bool mirrored=false
);

numeric::xyzVector<core::Real>
interp_dspline( ObjexxFCL::FArray3D< float > & coeffs ,
	numeric::xyzVector<core::Real> const & idxX,
	bool mirrored=false
);

/// @brief precompute spline coefficients (core::Real array => core::Real coeffs)
void spline_coeffs( ObjexxFCL::FArray3D< float > const &data, ObjexxFCL::FArray3D< core::Real > & coeffs, bool mirrored=false);

void conj_map_times(ObjexxFCL::FArray3D< std::complex<core::Real> > & map_product, ObjexxFCL::FArray3D< std::complex<core::Real> > const & mapA, ObjexxFCL::FArray3D< std::complex<core::Real> > const & mapB);

ObjexxFCL::FArray3D< core::Real > convolute_maps( ObjexxFCL::FArray3D< core::Real > const & mapA, ObjexxFCL::FArray3D< core::Real > const & mapB) ;


/// 4d interpolants
core::Real interp_spline(
	ObjexxFCL::FArray4D< core::Real > & coeffs ,
	core::Real slab,
	numeric::xyzVector< core::Real > const & idxX );

void interp_dspline(
	ObjexxFCL::FArray4D< core::Real > & coeffs ,
	numeric::xyzVector< core::Real > const & idxX ,
	core::Real slab,
	numeric::xyzVector< core::Real > & gradX,
	core::Real & gradSlab );
void spline_coeffs(
	ObjexxFCL::FArray4D< core::Real > const & data ,
	ObjexxFCL::FArray4D< core::Real > & coeffs);
void spline_coeffs(
	ObjexxFCL::FArray4D< float > const & data ,
	ObjexxFCL::FArray4D< core::Real > & coeffs);

///@brief Calculate the density and relative neighbor density score.
/// Map must be initialized to number of calculation residues.
void
calculate_density_nbr(
	pose::Pose & pose,
	std::map< Size, Real > & per_rsd_dens,
	std::map< Size, Real > & per_rsd_nbrdens,
	core::conformation::symmetry::SymmetryInfoCOP syminfo,
	bool mixed_sliding_window = false,
	Size sliding_window_size=3);

///@brief Calculate the geometry score using cartesian scoring.
/// Map must be initialized to number of calculation residues.
void
calculate_geometry(
	pose::Pose & pose,
	std::map< Size, Real > & geometry,
	Size n_symm_subunit,
	Real weight = 1.0 );

///@brief Calculate the geometry score using rama or sugar_bb
/// Map must be initialized to number of calculation residues.
void
calculate_rama(
	pose::Pose & pose,
	std::map< Size, Real > & rama,
	Size n_symm_subunit,
	Real weight = 0.2 );


///@brief Fill the weighted per-residue weighted score of a particular score type
/// Note, does not decompose pair-energies, but does work with symmetry
void
calc_per_rsd_score(
	pose::Pose & pose,
	scoring::ScoreType const & score_type,
	std::map<Size, Real > & per_rsd_score,
	Size n_symm_subunit,
	Real weight
);


/// @brief templated helper function to FFT resample a map
template<class S, class T>
void resample(
	ObjexxFCL::FArray3D< S > const &density,
	ObjexxFCL::FArray3D< T > &newDensity,
	numeric::xyzVector< int > newDims
) {
	if ( density.u1() == newDims[0] && density.u2() == newDims[1] && density.u3() == newDims[2] ) {
		newDensity = density;
		return;
	}

	newDensity.dimension( newDims[0], newDims[1], newDims[2] );

	// convert map to complex<core::Real>
	ObjexxFCL::FArray3D< std::complex<core::Real> > Foldmap, Fnewmap;
	Fnewmap.dimension( newDims[0], newDims[1], newDims[2] );

	// fft
	ObjexxFCL::FArray3D< std::complex<core::Real> > cplx_density;
	cplx_density.dimension( density.u1() , density.u2() , density.u3() );
	for ( int i=0; i<density.u1()*density.u2()*density.u3(); ++i ) cplx_density[i] = (core::Real)density[i];
	numeric::fourier::fft3(cplx_density, Foldmap);

	// reshape (handles both shrinking and growing in each dimension)
	for ( int i=0; i<Fnewmap.u1()*Fnewmap.u2()*Fnewmap.u3(); ++i ) Fnewmap[i] = std::complex<core::Real>(0,0);

	numeric::xyzVector<int> nyq( std::min(Foldmap.u1(), Fnewmap.u1())/2,
		std::min(Foldmap.u2(), Fnewmap.u2())/2,
		std::min(Foldmap.u3(), Fnewmap.u3())/2 );
	numeric::xyzVector<int> nyqplus1_old( std::max(Foldmap.u1() - (std::min(Foldmap.u1(),Fnewmap.u1())-nyq[0]) + 1 , nyq[0]+1) ,
		std::max(Foldmap.u2() - (std::min(Foldmap.u2(),Fnewmap.u2())-nyq[1]) + 1 , nyq[1]+1) ,
		std::max(Foldmap.u3() - (std::min(Foldmap.u3(),Fnewmap.u3())-nyq[2]) + 1 , nyq[2]+1) );
	numeric::xyzVector<int> nyqplus1_new( std::max(Fnewmap.u1() - (std::min(Foldmap.u1(),Fnewmap.u1())-nyq[0]) + 1 , nyq[0]+1) ,
		std::max(Fnewmap.u2() - (std::min(Foldmap.u2(),Fnewmap.u2())-nyq[1]) + 1 , nyq[1]+1) ,
		std::max(Fnewmap.u3() - (std::min(Foldmap.u3(),Fnewmap.u3())-nyq[2]) + 1 , nyq[2]+1) );
	for ( int i=1; i<=Fnewmap.u1(); i++ ) {
		for ( int j=1; j<=Fnewmap.u2(); j++ ) {
			for ( int k=1; k<=Fnewmap.u3(); k++ ) {
				if ( i-1<=nyq[0] ) {
					if ( j-1<=nyq[1] ) {
						if ( k-1<=nyq[2] ) {
							Fnewmap(i,j,k) = Foldmap(i, j, k);
						} else if ( k-1>=nyqplus1_new[2] ) {
							Fnewmap(i,j,k) = Foldmap(i, j, k-nyqplus1_new[2]+nyqplus1_old[2]);
						}

					} else if ( j-1>=nyqplus1_new[1] ) {
						if ( k-1<=nyq[2] ) {
							Fnewmap(i,j,k) = Foldmap(i, j-nyqplus1_new[1]+nyqplus1_old[1], k);
						} else if ( k-1>=nyqplus1_new[2] ) {
							Fnewmap(i,j,k) = Foldmap(i, j-nyqplus1_new[1]+nyqplus1_old[1], k-nyqplus1_new[2]+nyqplus1_old[2]);
						}
					}
				} else if ( i-1>=nyqplus1_new[0] ) {
					if ( j-1<=nyq[1] ) {
						if ( k-1<=nyq[2] ) {
							Fnewmap(i,j,k) = Foldmap(i-nyqplus1_new[0]+nyqplus1_old[0],j,k);
						} else if ( k-1>=nyqplus1_new[2] ) {
							Fnewmap(i,j,k) = Foldmap(i-nyqplus1_new[0]+nyqplus1_old[0],j, k-nyqplus1_new[2]+nyqplus1_old[2]);
						}

					} else if ( j-1>=nyqplus1_new[1] ) {
						if ( k-1<=nyq[2] ) {
							Fnewmap(i,j,k) = Foldmap(i-nyqplus1_new[0]+nyqplus1_old[0],j-nyqplus1_new[1]+nyqplus1_old[1],k);
						} else if ( k-1>=nyqplus1_new[2] ) {
							Fnewmap(i,j,k) = Foldmap(i-nyqplus1_new[0]+nyqplus1_old[0],
								j-nyqplus1_new[1]+nyqplus1_old[1],
								k-nyqplus1_new[2]+nyqplus1_old[2]);
						}
					}
				}
			}
		}
	}

	// ifft
	numeric::fourier::ifft3(Fnewmap, newDensity);
}


} // namespace constraints
} // namespace scoring
} // namespace core

#endif
