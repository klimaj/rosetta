// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/denovo_design/components/StructureDataPerturber.hh
/// @brief Classes for altering StructureData objects on the fly
/// @author Tom Linsky (tlinsky@uw.edu)
#ifndef INCLUDED_protocols_denovo_design_components_StructureDataPerturber_hh
#define INCLUDED_protocols_denovo_design_components_StructureDataPerturber_hh

// Unit headers
#include <protocols/denovo_design/components/StructureDataPerturber.fwd.hh>

// Protocol headers
#include <protocols/denovo_design/architects/HelixArchitect.fwd.hh>
#include <protocols/denovo_design/components/Segment.fwd.hh>
#include <protocols/denovo_design/components/StructureData.fwd.hh>
#include <protocols/denovo_design/components/VectorSelector.hh>
#include <protocols/denovo_design/connection/ConnectionArchitect.fwd.hh>
#include <protocols/denovo_design/types.hh>

// Utility headers
#include <basic/datacache/DataMap.fwd.hh>
#include <utility/tag/Tag.fwd.hh>
#include <utility/vector1.hh>

// C++ headers

namespace protocols {
namespace denovo_design {
namespace components {

/// @brief Classes for altering StructureData objects on the fly
class StructureDataPerturber  {
public:
	typedef SegmentCOPs Permutation;
	typedef utility::vector1< Permutation > Permutations;
	typedef EnumeratedVectorSelector< Permutation > PermutationSelector;

public: // Creation
	StructureDataPerturber();

	/// @brief Destructor
	virtual
	~StructureDataPerturber();

	virtual StructureDataPerturberOP
	clone() const = 0;

	virtual void
	parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data ) = 0;

	virtual Permutations
	enumerate( StructureData const & sd ) const = 0;

public:
	static StructureDataPerturberOP
	create( utility::tag::Tag const & tag, basic::datacache::DataMap & data );

	void
	apply( StructureData & sd );

	virtual void
	set_ignore_segments( SegmentNameSet const & ignore_set );

	//SegmentNameSet const &
	//ignore_segments() const;

	bool
	ignored( SegmentName const & segment_name ) const;

private:
	bool
	finished() const;

	/// @brief Replaces the segments in sd with those in perm
	void
	replace_segments( StructureData & sd, Permutation const & perm ) const;

private:
	SegmentNameSet ignore_;
	PermutationSelector permutations_;
}; // StructureDataPerturber

/// @brief Perturber that does nothing
class NullPerturber : public StructureDataPerturber {
public:
	// Creation

	/// @brief Default constructor
	NullPerturber();

	/// @brief Destructor
	~NullPerturber() override;

	StructureDataPerturberOP
	clone() const override;

	Permutations
	enumerate( StructureData const & sd ) const override;

	void
	parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data ) override;
};

/// @brief "mutates" a connection
class ConnectionPerturber : public StructureDataPerturber {
public:
	ConnectionPerturber();

	~ConnectionPerturber() override;

	static std::string
	class_name() { return "ConnectionPerturber"; }

	StructureDataPerturberOP
	clone() const override;

	void
	parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data ) override;

	Permutations
	enumerate( StructureData const & sd ) const override;

	/// @brief Sets the architect object that designs the connection to be perturbed
	void
	set_architect( connection::ConnectionArchitectCOP architect );

private:
	void
	retrieve_connection_architect( std::string const & arch_name, basic::datacache::DataMap & data );

private:
	connection::ConnectionArchitectCOP architect_;
};

/// @brief "mutates" a helix
class HelixPerturber : public StructureDataPerturber {
private:
	typedef architects::HelixArchitect HelixArchitect;
	typedef architects::HelixArchitectCOP HelixArchitectCOP;

public:
	HelixPerturber();

	~HelixPerturber() override;

	static std::string
	class_name() { return "HelixPerturber"; }

	StructureDataPerturberOP
	clone() const override;

	void
	parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data ) override;

	Permutations
	enumerate( StructureData const & sd ) const override;

	/// @brief Sets the architect object that designs the helix to be perturbed
	void
	set_architect( HelixArchitectCOP architect );

private:
	/// @brief Gets const pointer to helix architect from the data map and stores it as architect_
	void
	retrieve_helix_architect( std::string const & arch_name, basic::datacache::DataMap & data );

private:
	HelixArchitectCOP architect_;
};

/// @brief "mutates" a set of mixed architects
class CompoundPerturber : public StructureDataPerturber {
private:
	enum CombinationMode {
		AND = 1,
		OR,
		UNKNOWN
	};
public:
	typedef utility::vector1< Permutations > PermutationsByPerturber;

public:
	CompoundPerturber();

	/// @brief copy constructor -- necessary for deep copying of perturbers_
	CompoundPerturber( CompoundPerturber const & other );

	~CompoundPerturber() override;

	static std::string
	class_name() { return "CompoundPerturber"; }

	StructureDataPerturberOP
	clone() const override;

	void
	parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data ) override;

	Permutations
	enumerate( StructureData const & sd ) const override;

	/// @brief Sets list of segments that will be ignored by the perturber
	void
	set_ignore_segments( SegmentNameSet const & ignore_set ) override;

	/// @brief Add a sub-perturber. Based on the mode, one or all of the perturbers will be called every time a perturbation is requested.
	void
	add_perturber( StructureDataPerturberOP perturber );

	/// @brief Clears list of sub-perturber
	void
	clear_perturbers();

	/// @brief Sets the mode which defines how the compound perturber uses the sub-perturbers. AND=run all perturbers, OR=choose a perturber randomly and run only that one
	void
	set_mode( CombinationMode const mode );

private:
	Permutations
	all_combinations( PermutationsByPerturber const & all_motifs ) const;

private:
	CombinationMode mode_;
	StructureDataPerturberOPs perturbers_;
};

} //protocols
} //denovo_design
} //components


#endif //INCLUDED_protocols_denovo_design_components_StructureDataPerturber_hh
