// This is core/vbl/vbl_bit_array_2d.h
#ifndef vbl_bit_array_2d_h_
#define vbl_bit_array_2d_h_
//:
// \file
// \brief Contains class for a 2d bit array; interface as vbl_array_2d<T>
// \author Geoffrey Cross
//
// \verbatim
// Modifications
// Peter Vanroose - 4dec01 - adapted interface to that of vbl_array_2d<T>
// Peter Vanroose - 26 Aug 2004 - adapted interface to that of vbl_array_2d<T>
// \endverbatim
//-----------------------------------------------------------------------------

#include <iosfwd>
#ifdef _MSC_VER
#  include <vcl_msvc_warnings.h>
#endif

//: simple 2D bit array
// essentially identical to vbl_array_2d<bool> but more efficiently stored
class vbl_bit_array_2d
{
public:
  // Default constructor
  vbl_bit_array_2d() = default;
  //: Construct num_rows x num_cols array and leave data uninitialised
  vbl_bit_array_2d(unsigned int m, unsigned int n) { construct(m, n); }
  //: Construct num_rows x num_cols array and fill all cells with v
  vbl_bit_array_2d(unsigned int m, unsigned int n, bool v)
  {
    construct(m, n);
    fill(v);
  }
  //: Construct num_rows x num_cols array and fill all cells with v
  vbl_bit_array_2d(unsigned int m, unsigned int n, bool v[]);
  //: Copy constructor
  vbl_bit_array_2d(const vbl_bit_array_2d &);
  // Destructor
  ~vbl_bit_array_2d() { destruct(); }

  //: Assignment operator
  vbl_bit_array_2d &
  operator=(const vbl_bit_array_2d &);

  //: Comparison
  bool
  operator==(const vbl_bit_array_2d & a) const;
  //:
  bool
  operator!=(const vbl_bit_array_2d & a) const
  {
    return !operator==(a);
  }

  // Operations----------------------------------------------------------------

  //: Fill with value
  void
  fill(bool value);
  //: Delete contents and resize to m rows x n cols
  void
  resize(unsigned int m, unsigned int n)
  {
    destruct();
    construct(m, n);
  }
  //: Resizes and pads with zeros; keeps existing data
  void
  enlarge(unsigned int m, unsigned int n);
  //: make as if default-constructed.
  void
  clear()
  {
    if (data_)
    {
      destruct();
      construct(0, 0);
    }
  }

  // Data Access---------------------------------------------------------------
  bool
  operator()(unsigned int i, unsigned int j) const;
  bool
  operator()(unsigned int i, unsigned int j);

  void
  put(unsigned int i, unsigned int j, const bool & x);
  bool
  get(unsigned int i, unsigned int j) const;
  //: Set the value of a cell; default is to set the value on
  void
  set(unsigned int i, unsigned int j, bool v = true)
  {
    put(i, j, v);
  }
  //: Change the value of a cell
  void
  flip(unsigned int i, unsigned int j)
  {
    put(i, j, !get(i, j));
  }

  inline unsigned int
  rows() const
  {
    return num_rows_;
  }
  inline unsigned int
  cols() const
  {
    return num_cols_;
  }
  inline unsigned int
  columns() const
  {
    return num_cols_;
  }
  //: Number of bytes allocated by the data
  unsigned long
  size() const;

private:
  unsigned char * data_{ nullptr };
  unsigned int num_rows_{ 0 };
  unsigned int num_cols_{ 0 };

  void
  destruct()
  {
    delete[] data_;
    data_ = nullptr;
  }
  void
  construct(unsigned int m, unsigned int n);

  // helper
  void
  index(unsigned int x, unsigned int y, unsigned long & byteindex, unsigned int & bitindex) const;
};

std::ostream &
operator<<(std::ostream & os, const vbl_bit_array_2d & v);

#endif // vbl_bit_array_2d_h_
