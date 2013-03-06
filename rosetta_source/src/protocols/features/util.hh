// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet;
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file util.hh
///
/// @brief
/// @author Tim Jacobs

#ifndef INCLUDED_protocols_features_util_hh
#define INCLUDED_protocols_features_util_hh

#include <utility/sql_database/DatabaseSessionManager.hh>

#include <core/types.hh>
#include <core/conformation/Residue.fwd.hh>

#include <string>
#include <map>
#include <boost/uuid/uuid.hpp>

namespace protocols{
namespace features{

std::pair<core::Size, core::Size>
get_protocol_and_batch_id(
	std::string identifier,
	utility::sql_database::sessionOP db_session);

std::pair<core::Size, core::Size> deserialize_db_listener_data(std::string data);

std::string serialize_ids(int protocol_id, std::string identifier, core::Size batch_id);

core::Size
get_batch_id(
	boost::uuids::uuid struct_id,
	utility::sql_database::sessionOP db_session
);

std::string serialize_residue_xyz_coords(core::conformation::Residue const & residue);

utility::vector1< numeric::xyzVector<core::Real> > deserialize_xyz_coords(std::string const & data, core::Size natoms);

} //features
} //protocols

#endif
