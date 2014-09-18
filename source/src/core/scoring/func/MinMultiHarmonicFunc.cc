// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/core/scoring/func/HarmonicFunc.hh
/// @brief Definition for functions used in definition of constraints.
/// @author Dominik Gront (dgront@chem.uw.edu.pl)


#include <core/scoring/func/MinMultiHarmonicFunc.hh>
#include <core/types.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <basic/Tracer.hh>


// C++ Headers

namespace core {
namespace scoring {
namespace func {

static thread_local basic::Tracer trMinMultiHarmonicFunc( "fragment.picking.scores.MinMultiHarmonicFunc" );

MinMultiHarmonicFunc::MinMultiHarmonicFunc( utility::vector1<Real> const & x0_in, utility::vector1<Real> const & sd_in ) {
	n_ = x0_in.size(); //number of harmonics to consider
	assert(x0_in.size() == sd_in.size()); //each x0 gets its own sd
	x0_.clear();
	sd_.clear();
	for(Size i=1;i<=n_;++i) {
		x0_.push_back( x0_in[i] );
		sd_.push_back( sd_in[i] );
	}
}


Real
MinMultiHarmonicFunc::func( Real const x )  const {

	//find the value that minimizes (x-x0)^2 / sd^2 among all the possible harmonic funcs
	Real min = ( x-x0_[1] );
	min *= min/ (sd_[1] * sd_[1]);   //start at one
	which_component_ = 1;
	for(Size i=2;i<=x0_.size();++i) { //assumes at least 2 harmonic funcs are being considered
	  Real z = ( x-x0_[i] );
		z = z*z / (sd_[i] * sd_[i]);
	  if( z < min ) {
	    which_component_ = i;
	    min = z;
	  }
	}

	return min;  // returns min value of (x-x0)^2 / sd^2
}

Real
MinMultiHarmonicFunc::dfunc( Real const x ) const {

	Real min = ( x-x0_[1] );
	Real deriv = min / (sd_[1] * sd_[1]);
	min = min*deriv;              // min will be the minimum of (x-x0)^2 / sd^2
	which_component_ = 1;
	for(Size i=2;i<=x0_.size();++i) {
	  Real z = ( x-x0_[i] );
		Real const zderiv = z / (sd_[i] * sd_[i]);
		z = z*zderiv;              // z equals (x-x0)^2 / sd^2
	  if( z < min ) {
	    which_component_ = i;
	    min = z;
			deriv = zderiv;          // deriv = (x-x0) / sd^2  (actually it's 2x this value, as returned below)
	  }
	}

	return 2 * deriv;            // return 2 * (x-x0_) / ( sd_ * sd_ );
}

void
MinMultiHarmonicFunc::read_data( std::istream& in ) {

  x0_.clear();
  sd_.clear();
  Real r;
  std::string line;
  getline( in, line );
  std::istringstream line_stream( line );

  utility::vector1<Real> entries;
  do {
    line_stream >> r;
    entries.push_back( r );
  }
  while( !line_stream.fail() );
  n_ = entries.size() /2;
  if(n_*2 != entries.size() ) {
    trMinMultiHarmonicFunc.Warning<< "Expected even number of parameters but got"<<entries.size()<<
        "; the last value seems to be useless or one is missing"<<std::endl;
  }
  for(Size i=1;i<=n_;++i) {
    x0_.push_back( entries[i*2-1] );
    sd_.push_back( entries[i*2] );
  }
}

void
MinMultiHarmonicFunc::show_definition( std::ostream &out ) const {

	out << "MINMULTIHARMONIC";
	for(Size i=1;i<=n_;++i)
	   out<< " "<<x0_[i] << " " << sd_[i];
	out << std::endl;
}

Size
MinMultiHarmonicFunc::show_violations( std::ostream& out, Real x, Size verbose_level, Real threshold) const {
	if (verbose_level > 100 ) {
		out << "HARM " <<  this->func(x) << std::endl;
	} else if (verbose_level > 70 ) {
		if ( ( x < x0_[which_component_])  && ( this->func(x) > threshold ) ) out << "-";
		else if ( ( x > x0_[which_component_]) && ( this->func(x) > threshold )) out << "+";
		else out << ".";
	}
	return Func::show_violations( out, x, verbose_level, threshold);
}

} // namespace constraints
} // namespace scoring
} // namespace core
