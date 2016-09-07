// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/features/ProteinBackboneAtomAtomPairFeaturesCreator.hh
/// @brief  Header for ProteinBackboneAtomAtomPairFeaturesCreator for the ProteinBackboneAtomAtomPairFeatures load-time factory registration scheme
/// @author Matthew O'Meara

#ifndef INCLUDED_protocols_features_ProteinBackboneAtomAtomPairFeaturesCreator_hh
#define INCLUDED_protocols_features_ProteinBackboneAtomAtomPairFeaturesCreator_hh

// Unit Headers
#include <protocols/features/FeaturesReporterCreator.hh>

// c++ headers

#include <core/types.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace features {

/// @brief creator for the ProteinBackboneAtomAtomPairFeatures class
class ProteinBackboneAtomAtomPairFeaturesCreator : public FeaturesReporterCreator
{
public:
	ProteinBackboneAtomAtomPairFeaturesCreator();
	~ProteinBackboneAtomAtomPairFeaturesCreator() override;

	FeaturesReporterOP create_features_reporter() const override;
	std::string type_name() const override;
};

} //namespace features
} //namespace protocols

#endif
