// This is vxl/vnl/vnl_vector.txx

//
// Created: VDN 02/21/92 new lite version adapted from Matrix.h
//
// The parameterized vnl_vector<T> class implements 1D arithmetic vectors of a
// user specified type. The only constraint placed on the type is that
// it must overload the following operators: +, -, *, and /. Thus, it will
// be possible to have a vnl_vector over vcl_complex<T>.  The vnl_vector<T>
// class is static in size, that is once a vnl_vector<T> of a particular
// size has been declared, elements cannot be added or removed. Using the
// resize() method causes the vector to resize, but the contents will be
// lost.
//
// Each vector contains  a protected  data section  that has a  T* slot that
// points to the  physical memory allocated  for the one  dimensional array. In
// addition, an integer  specifies   the number  of  elements  for the
// vector.  These values  are provided in the  constructors.
//
// Several constructors are provided. See .h file for descriptions.
//
// Methods   are  provided   for destructive   scalar   and vector  addition,
// multiplication, check for equality  and inequality, fill, reduce, and access
// and set individual elements.  Finally, both  the  input and output operators
// are overloaded to allow for fomatted input and output of vector elements.
//
// vnl_vector is a special type of matrix, and is implemented for space and time
// efficiency. When vnl_vector is pre_multiplied by/with matrix, m*v, vnl_vector is
// implicitly a column matrix. When vnl_vector is post_multiplied by/with matrix
// v*m, vnl_vector is implicitly a row matrix.
//

#include "vnl_vector.h"

#include <vcl_cstdlib.h> // abort()
#include <vcl_vector.h>
#include <vcl_iostream.h>
#include <vcl_algorithm.h>

#include <vnl/vnl_math.h>
#include <vnl/vnl_complex.h>
#include <vnl/vnl_matrix.h>
#include <vnl/vnl_c_vector.h>
#include <vnl/vnl_numeric_traits.h>

//--------------------------------------------------------------------------------

// This macro allocates the dynamic storage used by a vnl_vector.
#define vnl_vector_alloc_blah(size) \
do { \
  this->num_elmts = (size); \
  this->data = vnl_c_vector<T>::allocate_T(this->num_elmts); \
} while(false)

// This macro deallocates the dynamic storage used by a vnl_vector.
#define vnl_vector_free_blah \
do { \
  vnl_c_vector<T>::deallocate(this->data, this->num_elmts); \
} while (false)

//: Creates a vector with specified length. O(n).
// Elements are not initialized.

template<class T>
vnl_vector<T>::vnl_vector (unsigned len)
{
  vnl_vector_alloc_blah(len);
}


//: Creates a vector of specified length, and initialize all elements with value. O(n).

template<class T>
vnl_vector<T>::vnl_vector (unsigned len, T const& value)
{
  vnl_vector_alloc_blah(len);
  for (unsigned i = 0; i < len; i ++)           // For each elmt
    this->data[i] = value;                      // Assign initial value
}

//: Creates a vector of specified length and initialize first n elements with values. O(n).

template<class T>
vnl_vector<T>::vnl_vector (unsigned len, int n, T const values[])
{
  vnl_vector_alloc_blah(len);
  if (n > 0) {                                  // If user specified values
    for (unsigned i = 0; i < len && n; i++, n--)        // Initialize first n elements
      this->data[i] = values[i];                // with values
  }
}

//: Creates a vector of length 3 and initializes with the arguments, x,y,z.

template<class T>
vnl_vector<T>::vnl_vector (T const& px, T const& py, T const& pz)
{
  vnl_vector_alloc_blah(3);
  this->data[0] = px;
  this->data[1] = py;
  this->data[2] = pz;
}

#if 0 // commented out
//: Creates a vector of specified length and initialize first n
// elements with values in ... O(n).  Arguments in ... can only be pointers,
// primitive types like int, and NOT OBJECTS passed by value, like vectors,
// matrices, because constructors must be known and called at compile time!!!

template<class T>
vnl_vector<T>::vnl_vector (unsigned len, int n, T v00, ...)
: num_elmts(len), data(vnl_c_vector<T>::allocate_T(len))
{
  cerr << "Please use automatic arrays instead variable arguments" << endl;
  if (n > 0) {                               // If user specified values
    va_list argp;                            // Declare argument list
    va_start (argp, v00);                    // Initialize macro
    for (int i = 0; i < len && n; i++, n--)  // For remaining values given
      if (i == 0)
     this->data[0] = v00;                    // Hack for v00 ...
      else
     this->data[i] = va_arg(argp, T);        // Extract and assign
    va_end(argp);
  }
}

template<class T>
vnl_vector<T>::vnl_vector (unsigned len, int n, T v00, ...)
: num_elmts(len), data(vnl_c_vector<T>::allocate_T(len))
{
  cerr << "Please use automatic arrays instead variable arguments" << endl;
  if (n > 0) {                                  // If user specified values
    va_list argp;                               // Declare argument list
    va_start (argp, v00);                       // Initialize macro
    for (int i = 0; i < len && n; i++, n--)     // For remaining values given
      if (i == 0)
        this->data[0] = v00;                    // Hack for v00 ...
      else
        this->data[i] = va_arg(argp, T);        // Extract and assign
    va_end(argp);
  }
}
#endif

//: Creates a new copy of vector v. O(n).
template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const& v)
{
  vnl_vector_alloc_blah(v.num_elmts);
  for (unsigned i = 0; i < v.num_elmts; i ++)   // For each element in v
    this->data[i] = v.data[i];                  // Copy value
}

//: Creates a vector from a block array of data, stored row-wise.
// Values in datablck are copied. O(n).

template<class T>
vnl_vector<T>::vnl_vector (T const* datablck, unsigned len)
{
  vnl_vector_alloc_blah(len);
  for (unsigned i = 0; i < len; ++i)    // Copy data from datablck
    this->data[i] = datablck[i];
}

//------------------------------------------------------------

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &u, vnl_vector<T> const &v, vnl_tag_add)
{
  vnl_vector_alloc_blah(u.num_elmts);
  if (u.size() != v.size())
    vnl_error_vector_dimension ("vnl_vector<>::vnl_vector(v, v, vnl_vector_add_tag)", u.size(), v.size());
  for (unsigned int i=0; i<num_elmts; ++i)
    data[i] = u[i] + v[i];
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &u, vnl_vector<T> const &v, vnl_tag_sub)
{
  vnl_vector_alloc_blah(u.num_elmts);
  if (u.size() != v.size())
    vnl_error_vector_dimension ("vnl_vector<>::vnl_vector(v, v, vnl_vector_sub_tag)", u.size(), v.size());
  for (unsigned int i=0; i<num_elmts; ++i)
    data[i] = u[i] - v[i];
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &u, T s, vnl_tag_mul)
{
  vnl_vector_alloc_blah(u.num_elmts);
  for (unsigned int i=0; i<num_elmts; ++i)
    data[i] = u[i] * s;
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &u, T s, vnl_tag_div)
{
  vnl_vector_alloc_blah(u.num_elmts);
  for (unsigned int i=0; i<num_elmts; ++i)
    data[i] = u[i] / s;
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &u, T s, vnl_tag_add)
{
  vnl_vector_alloc_blah(u.num_elmts);
  for (unsigned int i=0; i<num_elmts; ++i)
    data[i] = u[i] + s;
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &u, T s, vnl_tag_sub)
{
  vnl_vector_alloc_blah(u.num_elmts);
  for (unsigned int i=0; i<num_elmts; ++i)
    data[i] = u[i] - s;
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_matrix<T> const &M, vnl_vector<T> const &v, vnl_tag_mul)
{
  vnl_vector_alloc_blah(M.rows());
  if (M.cols() != v.size())
    vnl_error_vector_dimension ("vnl_vector<>::vnl_vector(M, v, vnl_vector_mul_tag)", M.cols(), v.size());
  for (unsigned int i=0; i<num_elmts; ++i) {
    T sum(0);
    for (unsigned int j=0; j<M.cols(); ++j)
      sum += M[i][j] * v[j];
    data[i] = sum;
  }
}

template<class T>
vnl_vector<T>::vnl_vector (vnl_vector<T> const &v, vnl_matrix<T> const &M, vnl_tag_mul)
{
  vnl_vector_alloc_blah(M.cols());
  if (v.size() != M.rows())
    vnl_error_vector_dimension ("vnl_vector<>::vnl_vector(v, M, vnl_vector_mul_tag)", v.size(), M.rows());
  for (unsigned int j=0; j<num_elmts; ++j) {
    T sum(0);
    for (unsigned int i=0; i<M.rows(); ++i)
      sum += v[i] * M[i][j];
    data[j] = sum;
  }
}

//: Frees up the array inside vector. O(1).

template<class T>
void vnl_vector<T>::destroy()
{
  vnl_vector_free_blah;
}

template<class T>
void vnl_vector<T>::clear()
{
  if (data) {
    destroy();
    num_elmts = 0;
    data = 0;
  }
}

template<class T>
bool vnl_vector<T>::resize(unsigned n)
{
  if (this->data) {
    // if no change in size, do not reallocate.
    if (this->num_elmts == n)
      return false;

    vnl_vector_free_blah;
    vnl_vector_alloc_blah(n);
  }
  else {
    // this happens if the vector is default constructed.
    vnl_vector_alloc_blah(n);
  }
  return true;
}

#undef vnl_vector_alloc_blah
#undef vnl_vector_free_blah

//------------------------------------------------------------

//: Read a vnl_vector from an ascii istream.
// If the vector has nonzero size on input, read that many values.
// Otherwise, read to EOF.
template <class T>
bool vnl_vector<T>::read_ascii(vcl_istream& s)
{
  bool size_known = (this->size() != 0);
  if (size_known) {
    for(unsigned i = 0; i < this->size(); ++i)
      s >> (*this)(i);
    return s.good() || s.eof();
  }

  // Just read until EOF
  vcl_vector<T> allvals;
  unsigned n = 0;
  while (!s.eof()) {
    T value;
    s >> value;

    if (!s.good())
      break;
    allvals.push_back(value);
    ++n;
  }
  this->resize(n); //*this = vnl_vector<T>(n);
  for(unsigned i = 0; i < n; ++i)
    (*this)[i] = allvals[i];
  return true;
}

template <class T>
vnl_vector<T> vnl_vector<T>::read(vcl_istream& s)
{
  vnl_vector<T> V;
  V.read_ascii(s);
  return V;
}

//: Sets all elements of a vector to a specified fill value. O(n).

template<class T>
void vnl_vector<T>::fill (T const& value) {
  for (unsigned i = 0; i < this->num_elmts; i++)
    this->data[i] = value;
}

//: Sets elements of a vector to those in an array. O(n).

template<class T>
void vnl_vector<T>::copy_in (T const *ptr) {
  for (unsigned i = 0; i < num_elmts; ++i)
    data[i] = ptr[i];
}

//: Sets elements of an array to those in vector. O(n).

template<class T>
void vnl_vector<T>::copy_out (T *ptr) const {
  for (unsigned i = 0; i < num_elmts; ++i)
    ptr[i] = data[i];
}

//: Copies rhs vector into lhs vector. O(n).
// Changes the dimension of lhs vector if necessary.

template<class T>
vnl_vector<T>& vnl_vector<T>::operator= (vnl_vector<T> const& rhs) {
  if (this != &rhs) {                           // make sure *this != m
    if (this->num_elmts != rhs.num_elmts) {
      vnl_c_vector<T>::deallocate(this->data, this->num_elmts); // Free up the data space
      this->num_elmts = rhs.num_elmts;          // Copy index specification
      this->data = vnl_c_vector<T>::allocate_T(this->num_elmts);        // Allocate the elements
    }
    for (unsigned i = 0; i < this->num_elmts; i++)      // For each index
      this->data[i] = rhs.data[i];              // Copy value
  }
  return *this;
}

//: Increments all elements of vector with value. O(n).

template<class T>
vnl_vector<T>& vnl_vector<T>::operator+= (T value) {
  for (unsigned i = 0; i < this->num_elmts; i++) // For each index
    this->data[i] += value;             // Add scalar
  return *this;
}

//: Multiplies all elements of vector with value. O(n).

template<class T>
vnl_vector<T>& vnl_vector<T>::operator*= (T value) {
  for (unsigned i = 0; i < this->num_elmts; i++) // For each index
    this->data[i] *= value;                     // Multiply by scalar
  return *this;
}

//: Divides all elements of vector by value. O(n).

template<class T>
vnl_vector<T>& vnl_vector<T>::operator/= (T value) {
  for (unsigned i = 0; i < this->num_elmts; i++) // For each index
    this->data[i] /= value;                     // division by scalar
  return *this;
}


//: Mutates lhs vector with its addition with rhs vector. O(n).

template<class T>
vnl_vector<T>& vnl_vector<T>::operator+= (vnl_vector<T> const& rhs) {
  if (this->num_elmts != rhs.num_elmts)         // Size?
    vnl_error_vector_dimension ("operator+=",
                           this->num_elmts, rhs.num_elmts);
  for (unsigned i = 0; i < this->num_elmts; i++) // For each index
    this->data[i] += rhs.data[i];               // Add elements
  return *this;
}


//:  Mutates lhs vector with its substraction with rhs vector. O(n).

template<class T>
vnl_vector<T>& vnl_vector<T>::operator-= (vnl_vector<T> const& rhs) {
  if (this->num_elmts != rhs.num_elmts)         // Size?
    vnl_error_vector_dimension ("operator-=",
                           this->num_elmts, rhs.num_elmts);
  for (unsigned i = 0; i < this->num_elmts; i++)
    this->data[i] -= rhs.data[i];
  return *this;
}

//: Pre-multiplies vector with matrix and stores result back in vector.
// v = m * v. O(m*n). Vector is assumed a column matrix.

template<class T>
vnl_vector<T>& vnl_vector<T>::pre_multiply (vnl_matrix<T> const& m) {
  if (m.columns() != this->num_elmts)           // dimensions do not match?
    vnl_error_vector_dimension ("operator*=",
                           this->num_elmts, m.columns());
  T* temp= vnl_c_vector<T>::allocate_T(m.rows()); // Temporary
  vnl_matrix<T>& mm = (vnl_matrix<T>&) m;       // Drop const for get()
  for (unsigned i = 0; i < m.rows(); i++) {     // For each index
    temp[i] = (T) 0.0;                          // Initialize element value
    for (unsigned k = 0; k < this->num_elmts; k++)      // Loop over column values
      temp[i] += (mm.get(i,k) * this->data[k]); // Multiply
  }
  vnl_c_vector<T>::deallocate(this->data, this->num_elmts); // Free up the data space
  num_elmts = m.rows();                         // Set new num_elmts
  this->data = temp;                            // Pointer to new storage
  return *this;                                 // Return vector reference
}

//: Post-multiplies vector with matrix and stores result back in vector.
// v = v * m. O(m*n). Vector is assumed a row matrix.

template<class T>
vnl_vector<T>& vnl_vector<T>::post_multiply (vnl_matrix<T> const& m) {
  if (this->num_elmts != m.rows())              // dimensions do not match?
    vnl_error_vector_dimension ("operator*=", this->num_elmts, m.rows());
  T* temp= vnl_c_vector<T>::allocate_T(m.columns()); // Temporary
  vnl_matrix<T>& mm = (vnl_matrix<T>&) m;       // Drop const for get()
  for (unsigned i = 0; i < m.columns(); i++) {  // For each index
    temp[i] = (T) 0.0;                          // Initialize element value
    for (unsigned k = 0; k < this->num_elmts; k++) // Loop over column values
      temp[i] += (this->data[k] * mm.get(k,i)); // Multiply
  }
  vnl_c_vector<T>::deallocate(this->data, num_elmts); // Free up the data space
  num_elmts = m.columns();                      // Set new num_elmts
  this->data = temp;                            // Pointer to new storage
  return *this;                                 // Return vector reference
}


//: Creates new vector containing the negation of THIS vector. O(n).

template<class T>
vnl_vector<T> vnl_vector<T>::operator- () const {
  vnl_vector<T> result(this->num_elmts);
  for (unsigned i = 0; i < this->num_elmts; i++)
    result.data[i] = - this->data[i];           // negate element
  return result;
}

#if 0 // commented out
//: Returns new vector which is the multiplication of matrix m with column vector v. O(m*n).

template<class T>
vnl_vector<T> operator* (vnl_matrix<T> const& m, vnl_vector<T> const& v) {

  if (m.columns() != v.size())                  // dimensions do not match?
    vnl_error_vector_dimension ("operator*",
                                m.columns(), v.size());
  vnl_vector<T> result(m.rows());               // Temporary
  vnl_matrix<T>& mm = (vnl_matrix<T>&) m;       // Drop const for get()
  for (unsigned i = 0; i < m.rows(); i++) {     // For each index
    result[i] = (T) 0.0;                        // Initialize element value
    for (unsigned k = 0; k < v.size(); k++)     // Loop over column values
      result[i] += (mm.get(i,k) * v[k]);        // Multiply
  }
  return result;
}


//: Returns new vector which is the multiplication of row vector v with matrix m. O(m*n).

template<class T>
vnl_vector<T> vnl_vector<T>::operator* (vnl_matrix<T> const&m) const {

  // rick@aai: casting away const avoids the following error (using gcc272)
  // at m.rows during instantiation of 'template class vnl_vector<double >;'
  // "cannot lookup method in incomplete type `const vnl_matrix<double>`"
  // For some reason, instantiating the following function prior to vnl_vector
  // also avoids the error.
  // template vnl_matrix<double > outer_product (const vnl_vector<double >&,const vnl_vector<dou

  if (num_elmts != m.rows())                    // dimensions do not match?
    vnl_error_vector_dimension ("operator*", num_elmts, m.rows());
  vnl_vector<T> result(m.columns());            // Temporary
  vnl_matrix<T>& mm = (vnl_matrix<T>&) m;       // Drop const for get()
  for (unsigned i = 0; i < m.columns(); i++) {  // For each index
    result.data[i] = (T) 0.0;                   // Initialize element value
    for (unsigned k = 0; k < num_elmts; k++)    // Loop over column values
      result.data[i] += (data[k] * mm.get(k,i)); // Multiply
  }
  return result;
}
#endif


//: Replaces elements with index begining at start, by values of v. O(n).

template<class T>
vnl_vector<T>& vnl_vector<T>::update (vnl_vector<T> const& v, unsigned start) {
 unsigned end = start + v.size();
  if (this->num_elmts < end)
    vnl_error_vector_dimension ("update", end-start, v.size());
  for (unsigned i = start; i < end; i++)
    this->data[i] = v.data[i-start];
  return *this;
}


//: Returns a subvector specified by the start index and length. O(n).

template<class T>
vnl_vector<T> vnl_vector<T>::extract (unsigned len, unsigned start) const {
  unsigned end = start + len;
  if (this->num_elmts < end)
    vnl_error_vector_dimension ("extract", end-start, len);
  vnl_vector<T> result(len);
  for (unsigned i = 0; i < len; i++)
    result.data[i] = data[start+i];
  return result;
}

//: Returns new vector whose elements are the products v1[i]*v2[i]. O(n).

template<class T>
vnl_vector<T> element_product (vnl_vector<T> const& v1, vnl_vector<T> const& v2) {
  if (v1.size() != v2.size())
    vnl_error_vector_dimension ("element_product", v1.size(), v2.size());
  vnl_vector<T> result(v1.size());
  for (unsigned i = 0; i < v1.size(); i++)
    result[i] = v1[i] * v2[i];
  return result;
}

//: Returns new vector whose elements are the quotients v1[i]/v2[i]. O(n).

template<class T>
vnl_vector<T> element_quotient (vnl_vector<T> const& v1, vnl_vector<T> const& v2) {
  if (v1.size() != v2.size())
    vnl_error_vector_dimension ("element_quotient",
                        v1.size(), v2.size());
  vnl_vector<T> result(v1.size());
  for (unsigned i = 0; i < v1.size(); i++)
    result[i] = v1[i] / v2[i];
  return result;
}

//:
template <class T>
vnl_vector<T> vnl_vector<T>::apply(T (*f)(T const&)) const {
  vnl_vector<T> ret(size());
  vnl_c_vector<T>::apply(this->data, num_elmts, f, ret.data);
  return ret;
}

//: Return the vector made by applying "f" to each element.
template <class T>
vnl_vector<T> vnl_vector<T>::apply(T (*f)(T)) const {
  vnl_vector<T> ret(num_elmts);
  vnl_c_vector<T>::apply(this->data, num_elmts, f, ret.data);
  return ret;
}

//: Returns the dot product of two nd-vectors, or [v1]*[v2]^T. O(n).

template<class T>
T dot_product (vnl_vector<T> const& v1, vnl_vector<T> const& v2) {
  if (v1.size() != v2.size())
    vnl_error_vector_dimension ("dot_product",
                                v1.size(), v2.size());
  return vnl_c_vector<T>::dot_product(v1.begin(),
                                      v2.begin(),
                                      v1.size());
}

//: Hermitian inner product. O(n)

template<class T>
T inner_product (vnl_vector<T> const& v1, vnl_vector<T> const& v2) {
  if (v1.size() != v2.size())
    vnl_error_vector_dimension ("inner_product",
                                v1.size(), v2.size());
  return vnl_c_vector<T>::inner_product(v1.begin(),
                                        v2.begin(),
                                        v1.size());
}

//: Returns the 'matrix element' <u|A|v> = u^t * A * v. O(mn).

template<class T>
T bracket(vnl_vector<T> const &u, vnl_matrix<T> const &A, vnl_vector<T> const &v) {
  if (u.size() != A.rows())
    vnl_error_vector_dimension("bracket",u.size(),A.rows());
  if (A.columns() != v.size())
    vnl_error_vector_dimension("bracket",A.columns(),v.size());
  T brak(0);
  for (unsigned i=0; i<u.size(); ++i)
    for (unsigned j=0; j<v.size(); ++j)
      brak += u[i]*A(i,j)*v[j];
  return brak;
}

//: Returns the nxn outer product of two nd-vectors, or [v1]^T*[v2]. O(n).

template<class T>
vnl_matrix<T> outer_product (vnl_vector<T> const& v1,
                             vnl_vector<T> const& v2) {
  vnl_matrix<T> out(v1.size(), v2.size());
  for (unsigned i = 0; i < out.rows(); i++)             // v1.column() * v2.row()
    for (unsigned j = 0; j < out.columns(); j++)
      out[i][j] = v1[i] * v2[j];
  return out;
}


//: Returns the cross-product of two 2d-vectors.

template<class T>
T cross_2d (vnl_vector<T> const& v1, vnl_vector<T> const& v2) {
  if (v1.size() < 2 || v2.size() < 2)
    vnl_error_vector_dimension ("cross_2d", v1.size(), v2.size());

  return v1[0] * v2[1] - v1[1] * v2[0];
}

//: Returns the 3X1 cross-product of two 3d-vectors.

template<class T>
vnl_vector<T> cross_3d (vnl_vector<T> const& v1, vnl_vector<T> const& v2) {
  if (v1.size() != 3 || v2.size() != 3)
    vnl_error_vector_dimension ("cross_3d", v1.size(), v2.size());
  vnl_vector<T> result(v1.size());

  vnl_vector<T>& vv1 = *((vnl_vector<T>*) &v1);
  vnl_vector<T>& vv2 = *((vnl_vector<T>*) &v2);

  result.x() = vv1.y() * vv2.z() - vv1.z() * vv2.y(); // work for both col/row
  result.y() = vv1.z() * vv2.x() - vv1.x() * vv2.z(); // representation
  result.z() = vv1.x() * vv2.y() - vv1.y() * vv2.x();
  return result;
}

//--------------------------------------------------------------------------------

template <class T>
void vnl_vector<T>::flip() {
  for (unsigned i=0;i<num_elmts/2;i++) {
    T tmp=data[i];
    data[i]=data[num_elmts-1-i];
    data[num_elmts-1-i]=tmp;
  }
}

template <class T>
void vnl_vector<T>::swap(vnl_vector<T> &that)
{
  vcl_swap(this->num_elmts, that.num_elmts);
  vcl_swap(this->data, that.data);
}

//--------------------------------------------------------------------------------

// fsm@robots : cos_angle should return a T, or a double-precision extension
// of T. "double" is wrong since it won't work if T is complex.
template <class T>
T cos_angle(vnl_vector<T> const& a, vnl_vector<T> const& b) {
  typedef typename vnl_numeric_traits<T>::real_t real_t;
  typedef typename vnl_numeric_traits<T>::abs_t abs_t;

  real_t ab = inner_product(a,b);
#if defined(VCL_VC50x)
  x"I don't believe vc50 needs this -- can someone confirm?";
  T a_b = sqrt( a.squared_magnitude() * b.squared_magnitude() );
  return T(ab)/T(a_b);
#else
  double/*abs_t*/ a_b = vcl_sqrt( double(a.squared_magnitude() * b.squared_magnitude()) );
  return T( ab * abs_t(1.0 / a_b) );
#endif
}

//: Returns smallest angle between two non-zero n-dimensional vectors. O(n).

template<class T>
double angle (vnl_vector<T> const& a, vnl_vector<T> const& b) {
  double cosine = vnl_math_abs( cos_angle(a, b) );
  return vcl_acos(cosine);
}

//
template <class T>
bool vnl_vector<T>::is_finite() const {
  for(unsigned i = 0; i < this->size();++i)
    if (!vnl_math_isfinite( (*this)[i] ))
      return false;

  return true;
}

//
template <class T>
void vnl_vector<T>::assert_finite() const {
  if (this->is_finite())
    return;

  vcl_cerr << "*** NAN FEVER **\n";
  vcl_cerr << *this;
  vcl_abort();
}

//
template <class T>
void vnl_vector<T>::assert_size(unsigned sz) const {
  if (this->size() != sz) {
    vcl_cerr << "vnl_vector : has size " << this->size() << ". Should be " << sz << vcl_endl;
    vcl_abort();
  }
}

template<class T>
bool vnl_vector<T>::operator_eq (vnl_vector<T> const& rhs) const {
  if (this == &rhs)                               // same object => equal.
    return true;

  if (this->size() != rhs.size())                 // Size different ?
    return false;                                 // Then not equal.
  for (unsigned i = 0; i < size(); i++)           // For each index
    if (!(this->data[i] == rhs.data[i]))          // Element different ?
      return false;                               // Then not equal.

  return true;                                    // Else same; return true.
}

//--------------------------------------------------------------------------------

//: Overloads the output operator to print a vector. O(n).

template<class T>
vcl_ostream& operator<< (vcl_ostream& s, vnl_vector<T> const& v) {
  for (unsigned i = 0; i < v.size()-1; ++i)   // For each index in vector
    s << v[i] << " ";                              // Output data element
  s << v[v.size()-1];
  return s;
}

//: Read a vnl_vector from an ascii istream.
// If the vector has nonzero size on input, read that many values.
// Otherwise, read to EOF.
template <class T>
vcl_istream& operator>>(vcl_istream& s, vnl_vector<T>& M) {
  M.read_ascii(s); return s;
}

template <class T>
void vnl_vector<T>::inline_function_tickler()
{
  vnl_vector<T> v;
  // fsm: hacks to get 2.96/2.97/3.0 to instantiate the inline functions.
  v = T(3) + v;
  v = T(3) - v;
  v = T(3) * v;
}


//--------------------------------------------------------------------------------

#define VNL_VECTOR_INSTANTIATE(T) \
template class vnl_vector<T >; \
/* arithmetic, comparison etc */ \
VCL_INSTANTIATE_INLINE(vnl_vector<T > operator+(T const, vnl_vector<T > const &)); \
VCL_INSTANTIATE_INLINE(vnl_vector<T > operator-(T const, vnl_vector<T > const &)); \
VCL_INSTANTIATE_INLINE(vnl_vector<T > operator*(T const, vnl_vector<T > const &)); \
template vnl_vector<T > operator*(vnl_matrix<T > const &, vnl_vector<T > const &); \
VCL_INSTANTIATE_INLINE(bool operator!=(vnl_vector<T > const & , vnl_vector<T > const & )); \
/* element-wise */ \
template vnl_vector<T > element_product(vnl_vector<T > const &, vnl_vector<T > const &); \
template vnl_vector<T > element_quotient(vnl_vector<T > const &, vnl_vector<T > const &); \
/* dot products, angles etc */ \
template T inner_product(vnl_vector<T > const &, vnl_vector<T > const &); \
template T dot_product(vnl_vector<T > const &, vnl_vector<T > const &); \
template T cos_angle(vnl_vector<T > const & , vnl_vector<T > const &); \
template double angle(vnl_vector<T > const & , vnl_vector<T > const &); \
template T bracket(vnl_vector<T > const &, vnl_matrix<T > const &, vnl_vector<T > const &); \
template vnl_matrix<T > outer_product(vnl_vector<T > const &,vnl_vector<T > const &); \
/* cross products */ \
template T cross_2d(vnl_vector<T > const &, vnl_vector<T > const &); \
template vnl_vector<T > cross_3d(vnl_vector<T > const &, vnl_vector<T > const &); \
/* I/O */ \
template vcl_ostream & operator<<(vcl_ostream &, vnl_vector<T > const &); \
template vcl_istream & operator>>(vcl_istream &, vnl_vector<T >       &)

