#ifndef vcsl_length_unit_h_
#define vcsl_length_unit_h_
//:
// \file
// \brief Abstract unit associated to the length dimension
// \author Francois BERTEL
//
// \verbatim
//  Modifications
//   2000/06/28 Francois BERTEL Creation. Adapted from IUE
//   2001/04/10 Ian Scott (Manchester) Converted perceps header to doxygen
//   2004/09/10 Peter Vanroose  Inlined all 1-line methods in class decl
//   2004/09/17 Peter Vanroose  do not pass vcsl_unit objects; use vcsl_unit_sptr instead
// \endverbatim

#include "vcsl_unit.h"
#include "vcsl_length_unit_sptr.h"
#include "vcsl_unit_sptr.h"

//: Abstract unit associated to the length dimension
class vcsl_length_unit : public vcsl_unit
{
  //***************************************************************************
  // Constructors/Destructor
  //***************************************************************************

protected:
  // Default constructor
  vcsl_length_unit() = default;

public:
  // Destructor
  ~vcsl_length_unit() override = default;

  //***************************************************************************
  // Status report
  //***************************************************************************

  //: Are `this' and `other' compatible units ?
  // (Are they associated to the same dimension ?)
  // Pure virtual function of vcsl_unit
  bool
  compatible_units(const vcsl_unit_sptr & other) const override;

  //: Return the standard unit associated to the dimension of `this'
  // Pure virtual function of vcsl_unit
  vcsl_unit_sptr
  standard_unit() const override;

  //***************************************************************************
  // Because VXL does not necessarily use dynamic_cast<>
  //***************************************************************************

  const vcsl_length_unit *
  cast_to_length_unit() const override
  {
    return this;
  }
};

#endif // vcsl_length_unit_h_
