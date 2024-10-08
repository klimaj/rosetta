// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/abinitio/abscript/JumpSampleDataCreator.hh
/// @brief This class will create instances of Mover AbscriptMover for the MoverFactory
/// @author Justin Porter

#ifndef INCLUDED_protocols_abinitio_abscript_movers_JumpSampleDataCreator_hh
#define INCLUDED_protocols_abinitio_abscript_movers_JumpSampleDataCreator_hh

#include <basic/datacache/WriteableCacheableDataCreator.hh>
#include <basic/datacache/WriteableCacheableData.fwd.hh>

namespace protocols {
namespace abinitio {
namespace abscript {

class JumpSampleDataCreator : public basic::datacache::WriteableCacheableDataCreator {
	typedef basic::datacache::WriteableCacheableDataOP WriteableCacheableDataOP;
public:
	WriteableCacheableDataOP create_data( std::istream &in ) const override;
	std::string keyname() const override;
};

}
}
}

#endif

