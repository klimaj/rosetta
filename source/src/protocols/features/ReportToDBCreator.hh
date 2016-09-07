// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/features/ReportToDBCreator.hh
/// @brief This class will create instances of Mover ReportToDB for the MoverFactory
/// @author Andrew Leaver-Fay via code_writer.py (aleaverfay@gmail.com)
/// @author Matthew O'Meara via (mattjomeara@gmail.com)

#ifndef INCLUDED_protocols_features_ReportToDBCreator_hh
#define INCLUDED_protocols_features_ReportToDBCreator_hh

#include <protocols/moves/MoverCreator.hh>

namespace protocols {
namespace features {

class ReportToDBCreator : public protocols::moves::MoverCreator {
public:
	moves::MoverOP create_mover() const override;
	std::string keyname() const override;
	static std::string mover_name();
};

}
}

#endif
