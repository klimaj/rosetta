// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/etable/coloumb/Coloumb.hh
/// @brief  evaluate Coloumbic potential
/// @author Phil Bradley, modifed by James Gleixner
/// @author Matthew O'Meara

#ifndef INCLUDED_core_scoring_coloumb_Coloumb_hh
#define INCLUDED_core_scoring_coloumb_Coloumb_hh

// Project headers
#include <core/scoring/etable/coloumb/Coloumb.fwd.hh>
#include <core/scoring/methods/EnergyMethodOptions.fwd.hh>
#include <core/scoring/etable/Etable.hh>

// Platform headers
#include <core/types.hh>

// Utility headers
#include <utility/vector1.hh>
#include <utility/pointer/ReferenceCount.hh>
namespace core {
namespace scoring {
namespace etable {
namespace coloumb {


class Coloumb : public utility::pointer::ReferenceCount
{

public:
	~Coloumb() {}

	Coloumb( methods::EnergyMethodOptions const & options );

	Coloumb( Coloumb const & src );

  /// @brief Initilize constants.
	void
	initialize();

	// was private, but outside world wants to call this one:
	inline
	Real
	eval_atom_atom_hack_elecE(
		Vector const & i_xyz,
		Real const i_charge,
		Vector const & j_xyz,
		Real const j_charge
	) const;

	inline
	Real
	eval_atom_atom_hack_elecE(
		Vector const & i_xyz,
		Real const i_charge,
		Vector const & j_xyz,
		Real const j_charge,
		DistanceSquared & d2
	) const;


	///@brief Get the key numeric value for derivative calculations
	/// i.e. the derivative of energy with respect to distance divided by the distance
	inline
	Real
	eval_dhack_elecE_dr_over_r(
		Real const dis2,
		Real const q1,
		Real const q2
	) const;


	inline Real max_dis( ) const { return max_dis_; }
	inline Real max_dis2( ) const { return max_dis2_;}
	inline Real min_dis( ) const { return min_dis_;}
	inline Real min_dis2( ) const { return min_dis2_;}
	inline bool smooth_hack_elec( ) const { return smooth_hack_elec_;}

private:

	Real max_dis_;
	Real max_dis2_;
	Real min_dis_;
	Real min_dis2_;

	bool smooth_hack_elec_; // use sigmoidal functions to eliminate derivative discontinuities?

	Real low_poly_start_;
	Real low_poly_start2_;
	Real low_poly_end_;
	Real low_poly_end2_;
	etable::SplineParameters low_poly_;
	Real low_poly_width_;
	Real low_poly_invwidth_;

	Real hi_poly_start_;
	Real hi_poly_start2_;
	Real hi_poly_end_;
	Real hi_poly_end2_;
	etable::SplineParameters hi_poly_;
	Real hi_poly_width_;
	Real hi_poly_invwidth_;


	Real die_;
	bool no_dis_dep_die_;

  ///@brief Precomputed constants
	Real C0_;
	Real C1_;
	Real C2_;
	Real min_dis_score_;
	Real dEfac_;


};

///////////////////////////////////////////////////////////////////////////////////
// The following functions are defined here in the header as they are inlined
// and are used in derived classes.

inline
Real
Coloumb::eval_atom_atom_hack_elecE(
	Vector const & i_xyz,
	Real const i_charge,
	Vector const & j_xyz,
	Real const j_charge
) const
{
	Real d2;
	return eval_atom_atom_hack_elecE(i_xyz, i_charge, j_xyz, j_charge, d2);
}

/// @brief Use a polynomial to smooth the transition between the
/// regions in which the score is changing and the regions
/// in which the score is held constant.
inline
Real
Coloumb::eval_atom_atom_hack_elecE(
	Vector const & i_xyz,
	Real const i_charge,
	Vector const & j_xyz,
	Real const j_charge,
	DistanceSquared & d2
) const
{
	d2 = i_xyz.distance_squared( j_xyz );

	if ( d2 > max_dis2_ ) {
		return 0.0;
	} else if ( d2 < low_poly_start2_ ) {
		return i_charge * j_charge * min_dis_score_;
	} else if ( d2 < low_poly_end2_ ) {
		return i_charge * j_charge * etable::Etable::eval_spline(
			std::sqrt( d2 ), low_poly_start_, low_poly_end_,
			low_poly_width_, low_poly_invwidth_, low_poly_ );
	} else if ( d2 > hi_poly_start2_ ) {
		return i_charge * j_charge * etable::Etable::eval_spline(
			std::sqrt( d2 ), hi_poly_start_, hi_poly_end_,
			hi_poly_width_, hi_poly_invwidth_, hi_poly_ );
	} else if ( no_dis_dep_die_ ) {
		return i_charge * j_charge * ( C1_ / std::sqrt(d2) - C2_ );
	} else {
		return i_charge * j_charge * ( C1_ / d2 - C2_ );
	}
}

////////////////////////////////////////////////////////////////////////////

inline
Real
Coloumb::eval_dhack_elecE_dr_over_r(
	Real const dis2,
	Real const q1,
	Real const q2
) const
{
	if ( dis2 > max_dis2_ ) return 0.0;
	else if ( dis2 < low_poly_start2_ ) return 0.0; // flat in this region

	Real q1q2 = q1*q2;

	if ( dis2 > low_poly_end2_ && dis2 < hi_poly_start2_ ) {
		if ( no_dis_dep_die_ ) {
			return dEfac_ * q1q2 / ( dis2 * std::sqrt(dis2) ) ;
		} else {
			return dEfac_ * q1q2 / ( dis2 * dis2 );
		}
	} else if ( dis2 < low_poly_end2_ ) {
		Real d = std::sqrt( dis2 );
		return etable::Etable::spline_deriv(
			d, low_poly_start_, low_poly_end_,
			low_poly_width_, low_poly_invwidth_, low_poly_ ) * q1q2 / d;
	} else {
		Real d = std::sqrt( dis2 );
		return etable::Etable::spline_deriv(
			d, hi_poly_start_, hi_poly_end_,
			hi_poly_width_, hi_poly_invwidth_, hi_poly_ ) * q1q2 / d;
	}
}

////////////////////////////////////////////////////////////////////////////

}
}
}
}
#endif //include guard
