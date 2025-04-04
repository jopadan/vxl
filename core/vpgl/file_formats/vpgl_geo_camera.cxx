// This is core/vpgl/file_formats/vpgl_geo_camera.cxx
#include <vector>
#include <array>
#include <stdexcept>
#include "vpgl_geo_camera.h"
//:
// \file

#ifdef _MSC_VER
#  include "vcl_msvc_warnings.h"
#endif
#include <cassert>

#include "vnl/vnl_vector.h"
#include "vnl/vnl_inverse.h"

#include "vpgl/vpgl_lvcs.h"
#include "vpgl/vpgl_utm.h"
#include "vul/vul_file.h"

#if HAS_GEOTIFF
#  include "vil/vil_load.h"
#  include <vil/file_formats/vil_geotiff_header.h>
#  include <vil/file_formats/vil_tiff.h>
#  include <vil/file_formats/vil_nitf2_image.h>
#endif

vpgl_geo_camera::vpgl_geo_camera()
  : sx_(0.0)
  , sy_(0.0)
{
  trans_matrix_.set_size(4, 4);
  trans_matrix_.fill(0);
  trans_matrix_.fill_diagonal(1);
  is_utm_ = false;
  scale_tag_ = false;
}

vpgl_geo_camera::vpgl_geo_camera(const vpgl_geo_camera & rhs)
  : vpgl_camera<double>(rhs)
  , trans_matrix_(rhs.trans_matrix_)
  , is_utm_(rhs.is_utm_)
  , utm_zone_(rhs.utm_zone_)
  , northing_(rhs.northing_)
  , scale_tag_(rhs.scale_tag_)
{
  this->set_lvcs(rhs.lvcs_);
  rhs.pixel_spacing(sx_, sy_);
}


// camera initialization requiring GEOTIFF capabilities
#if HAS_GEOTIFF

// Load camera from geotiff file
bool
vpgl_geo_camera::load_from_geotiff(const std::string & file, const vpgl_lvcs * lvcs)
{
  auto resource = vil_load_image_resource(file.c_str());
  return this->load_from_resource(resource, lvcs);
}

// Load camera from geotiff resource
bool
vpgl_geo_camera::load_from_resource(const vil_image_resource_sptr & geotiff_img, const vpgl_lvcs * lvcs)
{
  // check if the image is tiff
  auto * geotiff_tiff = dynamic_cast<vil_tiff_image *>(geotiff_img.ptr());
  if (!geotiff_tiff)
  {
    std::cerr << "vpgl_geo_camera::load_from_geotiff -- Error casting vil_image_resource to a tiff image.\n";
    return false;
  }

  // check if the tiff file is geotiff
  if (!geotiff_tiff->is_GEOTIFF())
  {
    std::cerr << "vpgl_geo_camera::load_from_geotiff -- The image should be a GEOTIFF!\n";
    return false;
  }

  // retrieve header
  vil_geotiff_header * gtif = geotiff_tiff->get_geotiff_header();
  if (!gtif)
  {
    std::cerr << "vpgl_geo_camera::load_from_geotiff -- no geotiff header!\n";
    return false;
  }

  int utm_zone;
  vil_geotiff_header::GTIF_HEMISPH h;

  std::vector<std::vector<double>> tiepoints;
  gtif->gtif_tiepoints(tiepoints);

  // create a transformation matrix
  // if there is a transformation matrix in GEOTIFF, use that
  vnl_matrix<double> trans_matrix(4, 4, 0.0);
  double * trans_matrix_values;
  double sx1, sy1, sz1;
  bool scale_tag = false;
  if (gtif->gtif_trans_matrix(trans_matrix_values))
  {
    std::cout << "Transfer matrix is given, using that...." << std::endl;
    trans_matrix.copy_in(trans_matrix_values);
    std::cout << "Warning LIDAR sample spacing different than 1 meter will not be handled correctly!\n";
  }
  else if (gtif->gtif_pixelscale(sx1, sy1, sz1))
  {
    scale_tag = true;
    vpgl_geo_camera::comp_trans_matrix(sx1, sy1, sz1, tiepoints, trans_matrix, scale_tag);
  }
  else
  {
    std::cerr << "vpgl_geo_camera::load_from_geotiff -- comp_trans_matrix -- Transform matrix cannot be formed..\n";
    return false;
  }

  // validate trans_matrix
  if ((trans_matrix.rows() != 4) || (trans_matrix.cols() != 4))
  {
    std::cerr << "vpgl_geo_camera::load_from_geotiff -- requires 4x4 transform matrix\n";
    return false;
  }

  // update the camera
  this->trans_matrix_ = trans_matrix;
  this->scale_tag_ = scale_tag;
  this->is_utm_ = false;
  this->set_lvcs(lvcs);


  // check if the model type is geographic and also the units
  if (gtif->GCS_WGS84_MET_DEG())
  {
    this->extract_pixel_size();
    return true;
  }

  // otherwise check if it is projected to UTM and figure out the zone
  if (gtif->PCS_WGS84_UTM_zone(utm_zone, h) || gtif->PCS_NAD83_UTM_zone(utm_zone, h))
  {
    this->set_utm(utm_zone, h);
    this->extract_pixel_size();
    return true;
  }
  else
  {
    std::cerr << "vpgl_geo_camera::load_from_geotiff -- if UTM only PCS_WGS84_UTM and PCS_NAD83_UTM, if geographic "
                 "(GCS_WGS_84) "
                 "only linear units in meters, angular units in degrees are supported"
              << std::endl;
    return false;
  }
}

// static function to initialize geo camera on the heap
// maintain for backward compatability
bool
vpgl_geo_camera::init_geo_camera(const vil_image_resource_sptr & geotiff_img,
                                 const vpgl_lvcs_sptr & lvcs,
                                 vpgl_geo_camera *& camera)
{
  vpgl_geo_camera cam;
  if (!cam.load_from_resource(geotiff_img, lvcs.ptr()))
  {
    camera = nullptr;
    return false;
  }
  else
  {
    camera = cam.clone();
    return true;
  }
}

#endif // HAS_GEOTIFF

//: Load camera from GDAL geotransform
// https://gdal.org/user/raster_data_model.html#affine-geotransform
//
// The affine GDAL GeoTransform is defined as
//   Xgeo = GT(0) + Xpixel*GT(1) + Yline*GT(2)
//   Ygeo = GT(3) + Xpixel*GT(4) + Yline*GT(5)
//
// This relates to the vpgl_geo_camera trans_matrix as follows
//   | X |   | GT(1)  GT(2)  0  GT(0) |  | I |
//   | Y | = | GT(4)  GT(5)  0  GT(3) |  | J |
//   | Z |   | 0      0      1  0     |  | K |
//   | 1 |   | 0      0      0  1     |  | 1 |
// Where I==XPixel (column coordinate), J==YLine (row coordinate), and Z==K.
//
// Note GeoTransform pixel/line coordinates assume "PixelIsArea",
// where (0.0, 0.0) is the top left corner of the top left pixel.
// The location of the center of the top left pixel is at (0.5, 0.5).
bool
vpgl_geo_camera::load_from_geotransform(std::array<double, 6> geotransform,
                                        int utm_zone,
                                        int northing,
                                        const vpgl_lvcs * lvcs)
{
  vnl_matrix_fixed<double, 4, 4> trans_matrix;
  trans_matrix.fill(0);
  trans_matrix.fill_diagonal(1);

  // set geotransform items
  trans_matrix[0][3] = geotransform[0];
  trans_matrix[0][0] = geotransform[1];
  trans_matrix[0][1] = geotransform[2];
  trans_matrix[1][3] = geotransform[3];
  trans_matrix[1][0] = geotransform[4];
  trans_matrix[1][1] = geotransform[5];

  // update object
  this->trans_matrix_ = trans_matrix.as_matrix();
  this->scale_tag_ = true;
  this->is_utm_ = (utm_zone > 0);
  this->utm_zone_ = utm_zone;
  this->northing_ = northing;
  this->set_lvcs(lvcs);
  this->extract_pixel_size();
  return true;
}


//: define a geo_camera by the image file name (filename should have format such as xxx_N35W73_S0.6x0.6_xxx.tif)
bool
vpgl_geo_camera::init_geo_camera(const std::string & img_name,
                                 unsigned ni,
                                 unsigned nj,
                                 const vpgl_lvcs_sptr & lvcs,
                                 vpgl_geo_camera *& camera)
{
  // determine the translation matrix from the image file name and construct a geo camera
  std::string name = vul_file::strip_directory(img_name);
  name = name.substr(name.find_first_of('_') + 1, name.size());

  std::string n_coords = name.substr(0, name.find_first_of('_'));
  std::string n_scale = name.substr(name.find_first_of('_') + 1, name.find_last_of('_') - name.find_first_of('_') - 1);
  std::cout << "will determine transformation matrix from the file name: " << name << std::endl;

  // determine the lat, lon, hemisphere (North or South) and direction (East or West)
  std::string hemisphere, direction;
  float lon, lat, scale_lat, scale_lon;
  std::size_t n = n_coords.find('N');
  if (n < n_coords.size())
    hemisphere = "N";
  else
    hemisphere = "S";
  n = n_coords.find('E');
  if (n < n_coords.size())
    direction = "E";
  else
    direction = "W";

  std::string n_str = n_coords.substr(n_coords.find_first_of(hemisphere) + 1,
                                      n_coords.find_first_of(direction) - n_coords.find_first_of(hemisphere) - 1);
  std::stringstream str(n_str);
  str >> lat;

  n_str = n_coords.substr(n_coords.find_first_of(direction) + 1, n_coords.size());
  std::stringstream str2(n_str);
  str2 >> lon;

  n_str = n_scale.substr(n_scale.find_first_of('S') + 1, n_scale.find_first_of('x') - n_scale.find_first_of('S') - 1);
  std::stringstream str3(n_str);
  str3 >> scale_lat;

  n_str = n_scale.substr(n_scale.find_first_of('x') + 1, n_scale.size());
  std::stringstream str4(n_str);
  str4 >> scale_lon;

  std::cout << " hemisphere: " << hemisphere << " direction: " << direction << "\n lat: " << lat << " lon: " << lon
            << "\n scale_lat: " << scale_lat << " scale_lon: " << scale_lon << std::endl;

  // determine the upper left corner to use a vpgl_geo_cam, subtract from lat
  if (hemisphere == "N")
    std::cout << " upper left corner in the image is: " << hemisphere << lat + scale_lat << direction << lon
              << std::endl;
  else
    std::cout << " upper left corner in the image is: " << hemisphere << lat - scale_lat << direction << lon
              << std::endl;
  if (direction == "W")
    std::cout << " lower right corner in the image is: " << hemisphere << lat << direction << lon - scale_lon
              << std::endl;
  else
    std::cout << " lower right corner in the image is: " << hemisphere << lat << direction << lon + scale_lon
              << std::endl;
  vnl_matrix<double> trans_matrix(4, 4, 0, nullptr);
  // divide by ni-1 to account for 1 pixel overlap with the next tile
  if (direction == "E")
  {
    trans_matrix[0][3] = lon - 0.5 / (ni - 1.0);
    trans_matrix[0][0] = scale_lon / (ni - 1.0);
  }
  else
  {
    trans_matrix[0][3] = lon + 0.5 / (ni - 1.0);
    trans_matrix[0][0] = -scale_lon / (ni - 1.0);
  }
  if (hemisphere == "N")
  {
    trans_matrix[1][1] = -scale_lat / (nj - 1.0);
    trans_matrix[1][3] = lat + scale_lat + 0.5 / (nj - 1.0);
  }
  else
  {
    trans_matrix[1][1] = scale_lat / (nj - 1.0);
    trans_matrix[1][3] = lat - scale_lat - 0.5 / (nj - 1.0);
  }
  camera = new vpgl_geo_camera(trans_matrix, lvcs);
  camera->set_scale_format(true);
  camera->extract_pixel_size();
  return true;

#if 0
    std::string n = name.substr(name.find_first_of('N')+1, name.find_first_of('W'));
  float lon, lat, scale;
  std::stringstream str(n); str >> lat;
  n = name.substr(name.find_first_of('W')+1, name.find_first_of('_'));
  std::stringstream str2(n); str2 >> lon;
  n = name.substr(name.find_first_of('x')+1, name.find_last_of('.'));
  std::stringstream str3(n); str3 >> scale;
  std::cout << " lat: " << lat << " lon: " << lon << " WARNING: using same scale for both ni and nj: scale:" << scale << std::endl;

  // determine the upper left corner to use a vpgl_geo_cam, subtract from lat
  std::cout << "upper left corner in the image is: " << lat+scale << " N " << lon << " W\n"
           << "lower right corner in the image is: " << lat << " N " << lon-scale << " W" << std::endl;
  vnl_matrix<double> trans_matrix(4,4,0.0);
  trans_matrix[0][0] = -scale/ni; trans_matrix[1][1] = -scale/nj;
  trans_matrix[0][3] = lon; trans_matrix[1][3] = lat+scale;
  camera = new vpgl_geo_camera(trans_matrix, lvcs);
  camera->set_scale_format(true);
  return true;
#endif
}

// loads a geo_camera from the file and uses global WGS84 coordinates, so no need to convert negative values to
// positives in the global_to_img method as in the previous method
bool
vpgl_geo_camera::init_geo_camera_from_filename(const std::string & img_name,
                                               unsigned ni,
                                               unsigned nj,
                                               const vpgl_lvcs_sptr & lvcs,
                                               vpgl_geo_camera *& camera)
{
  // determine the translation matrix from the image file name and construct a geo camera
  std::string name = vul_file::strip_directory(img_name);
  name = name.substr(name.find_first_of('_') + 1, name.size());

  std::string n_coords = name.substr(0, name.find_first_of('_'));
  std::string n_scale = name.substr(name.find_first_of('_') + 1, name.find_last_of('_') - name.find_first_of('_') - 1);
  std::cout << "will determine transformation matrix from the file name: " << name << std::endl;

  // determine the lat, lon, hemisphere (North or South) and direction (East or West)
  std::string hemisphere, direction;
  float lon, lat, scale;
  std::size_t n = n_coords.find('N');
  if (n < n_coords.size())
    hemisphere = "N";
  else
    hemisphere = "S";
  n = n_coords.find('E');
  if (n < n_coords.size())
    direction = "E";
  else
    direction = "W";

  std::string n_str = n_coords.substr(n_coords.find_first_of(hemisphere) + 1,
                                      n_coords.find_first_of(direction) - n_coords.find_first_of(hemisphere) - 1);
  std::stringstream str(n_str);
  str >> lat;

  n_str = n_coords.substr(n_coords.find_first_of(direction) + 1, n_coords.size());
  std::stringstream str2(n_str);
  str2 >> lon;

  n_str = n_scale.substr(n_scale.find_first_of('S') + 1, n_scale.find_first_of('x') - n_scale.find_first_of('S') - 1);
  std::stringstream str3(n_str);
  str3 >> scale;

  std::cout << " hemisphere: " << hemisphere << " direction: " << direction << "\n lat: " << lat << " lon: " << lon
            << "\n WARNING: using same scale for both ni and nj: " << scale << std::endl;

  // simply convert to global coords (i.e. lats for S are negative)
  if (hemisphere == "S")
    lat = -lat;
  if (direction == "W")
    lon = -lon;

  // determine the upper left corner to use a vpgl_geo_cam, subtract from lat
  std::cout << " upper left corner in the image is: " << hemisphere << lat + scale << direction << lon << std::endl;
  std::cout << " lower right corner in the image is: " << hemisphere << lat << direction << lon + scale << std::endl;

  vnl_matrix<double> trans_matrix(4, 4, 0, nullptr);
  // divide by ni-1 to account for 1 pixel overlap with the next tile
  trans_matrix[0][3] = lon - 0.5 / (ni - 1.0);
  trans_matrix[0][0] = scale / (ni - 1.0);
  trans_matrix[1][1] = -scale / (nj - 1.0);
  trans_matrix[1][3] = lat + scale + 0.5 / (nj - 1.0);
  camera = new vpgl_geo_camera(trans_matrix, lvcs);
  camera->set_scale_format(true);
  camera->extract_pixel_size();
  return true;
}


//: init using a tfw file, reads the transformation matrix from the tfw
bool
vpgl_geo_camera::init_geo_camera(const std::string & tfw_name,
                                 const vpgl_lvcs_sptr & lvcs,
                                 int utm_zone,
                                 unsigned northing,
                                 vpgl_geo_camera *& camera)
{

  std::ifstream ifs(tfw_name.c_str());

  if (!ifs.is_open())
  {
    std::cerr << "in vpgl_geo_camera::init_geo_camera() -- cannot open: " << tfw_name << '\n';
    return false;
  }

  vnl_matrix<double> trans_matrix(4, 4, 0.0);
  ifs >> trans_matrix[0][0];
  ifs >> trans_matrix[0][1];
  ifs >> trans_matrix[1][0];
  ifs >> trans_matrix[1][1];
  ifs >> trans_matrix[0][3];
  ifs >> trans_matrix[1][3];
  trans_matrix[3][3] = 1.0;

  camera = new vpgl_geo_camera(trans_matrix, lvcs);
  if (utm_zone != 0)
    camera->set_utm(utm_zone, northing);
  camera->set_scale_format(true);
  camera->extract_pixel_size();
  ifs.close();
  return true;
}
void
vpgl_geo_camera::extract_pixel_size()
{
  if (is_utm_ && this->scale_tag_)
  {
    sx_ = trans_matrix_[0][0];
    sy_ = fabs(trans_matrix_[1][1]); // can be negative
    return;
  }
  vpgl_lvcs lvcs;
  if (lvcs_)
    lvcs = vpgl_lvcs(*lvcs_);
  else
  {
    double lon, lat;
    this->img_to_global(0.0, 0.0, lon, lat);
    lvcs = vpgl_lvcs(lat, lon, 0.0, vpgl_lvcs::wgs84, vpgl_lvcs::DEG, vpgl_lvcs::METERS);
  }
  double lon0, lat0, lonx, latx, lony, laty;
  this->img_to_global(0.0, 0.0, lon0, lat0);
  this->img_to_global(100000.0, 0.0, lonx, latx);
  this->img_to_global(0.0, 100000.0, lony, laty);
  double dlx0, dlx1, dly0, dly1, dlz;
  lvcs.global_to_local(double(lon0), double(lat0), double(0), vpgl_lvcs::wgs84, dlx0, dly0, dlz);
  lvcs.global_to_local(double(lonx), double(latx), double(0), vpgl_lvcs::wgs84, dlx1, dly1, dlz);
  sx_ = sqrt((dlx1 - dlx0) * (dlx1 - dlx0) + (dly1 - dly0) * (dly1 - dly0)) / 100000.0;
  lvcs.global_to_local(double(lony), double(laty), double(0), vpgl_lvcs::wgs84, dlx1, dly1, dlz);
  sy_ = sqrt((dlx1 - dlx0) * (dlx1 - dlx0) + (dly1 - dly0) * (dly1 - dly0)) / 100000.0;
}
//: transforms a given local 3d world point to global geo coordinates
void
vpgl_geo_camera::local_to_global(double lx, double ly, double lz, double & gx, double & gy, double & gz) const
{
  if (lvcs_)
  {
    if (lvcs_->get_cs_name() == vpgl_lvcs::utm)
    {
      if (is_utm_)
      { // geo cam is also utm so keep using utm
        lvcs_->local_to_global(lx, ly, lz, vpgl_lvcs::utm, gx, gy, gz);
      }
      else
      { // geo cam is not utm, convert to wgs84 as global
        lvcs_->local_to_global(lx, ly, lz, vpgl_lvcs::wgs84, gx, gy, gz);
      }
    }
    else
    {
      lvcs_->local_to_global(lx, ly, lz, vpgl_lvcs::wgs84, gx, gy, gz);
    }
  }
  else
  { // if there is no lvcs, then we assume global coords are given in wgs84, i.e. x is lon and y is lat
    gx = lx;
    gy = ly;
    gz = lz;
  }
}

bool
vpgl_geo_camera::global_to_local(double gx, double gy, double gz, double & lx, double & ly, double & lz) const
{
  if (!lvcs_)
  {
    std::cout << "No local vertical CS defined - can't map local to global" << std::endl;
    return false;
  }
  if (lvcs_->get_cs_name() == vpgl_lvcs::utm)
  {
    lvcs_->global_to_local(gx, gy, gz, vpgl_lvcs::utm, lx, ly, lz);
    return true;
  }
  if (lvcs_->get_cs_name() == vpgl_lvcs::wgs84)
  {
    lvcs_->global_to_local(gx, gy, gz, vpgl_lvcs::wgs84, lx, ly, lz);
    return true;
  }
  std::cout << "lvcs name " << lvcs_->get_cs_name() << " not handled " << std::endl;
  return false;
}

double
vpgl_geo_camera::lvcs_elev_origin() const
{
  if (!lvcs_)
    return 0.0;
  double ox, oy, oz;
  int zone;
  if (lvcs_->get_cs_name() == vpgl_lvcs::utm)
  {
    lvcs_->get_utm_origin(ox, oy, oz, zone);
    return oz;
  }
  if (lvcs_->get_cs_name() == vpgl_lvcs::wgs84)
  {
    lvcs_->get_origin(oy, ox, oz);
    return oz;
  }
  return 0.0;
}

//: transforms a given local 3d world point to image plane
void
vpgl_geo_camera::project(const double x, const double y, const double z, double & u, double & v) const
{
  double lat, lon, gz;
  if (lvcs_)
  {
    if (lvcs_->get_cs_name() == vpgl_lvcs::utm)
    {
      if (is_utm_)
      { // geo cam is also utm so keep using utm
        double gx, gy;
        this->local_to_global(x, y, z, gx, gy, gz);
        this->global_utm_to_img(gx, gy, utm_zone_, gz, u, v);
      }
      else
      { // geo cam is not utm, convert to wgs84 as global
        this->local_to_global(x, y, z, lon, lat, gz);
        this->global_to_img(lon, lat, gz, u, v);
      }
    }
    else
    {
      this->local_to_global(x, y, z, lon, lat, gz);
      this->global_to_img(lon, lat, gz, u, v);
    }
  }
  else
  { // if there is no lvcs, then we assume global coords are given in wgs84, i.e. x is lon and y is lat
    this->global_to_img(x, y, z, u, v);
  }
}

//: backprojects an image point into local coordinates (based on lvcs_)
void
vpgl_geo_camera::backproject(const double u, const double v, double & x, double & y, double & z) const
{
  vnl_vector<double> vec(4), res(4);
  if (scale_tag_)
  {
    vec[0] = trans_matrix_[0][3] + trans_matrix_[0][0] * u;
    vec[1] = trans_matrix_[1][3] + trans_matrix_[1][1] * v;
  }
  else
  { // assumes scale is 1
    vec[0] = trans_matrix_[0][3] + u;
    vec[1] = trans_matrix_[1][3] - v;
  }
  vec[2] = 0;
  vec[3] = 1;
  // std::cout << '\n' << vec << std::endl;

  double lat, lon, elev;
  if (is_utm_)
  {
    if (lvcs_)
    {
      if (lvcs_->get_cs_name() == vpgl_lvcs::utm)
      { // the local cs of lvcs is also utm, so use it directly
        lvcs_->global_to_local(vec[0], vec[1], vec[2], vpgl_lvcs::utm, x, y, z);
        return;
      }
    }
    // find the UTM values
    vpgl_utm utm;
    utm.transform(utm_zone_, vec[0], vec[1], vec[2], lat, lon, elev);
  }
  else
  {
    lon = vec[0];
    lat = vec[1];
    elev = vec[2];
  }

  if (lvcs_)
    lvcs_->global_to_local(lon, lat, elev, vpgl_lvcs::wgs84, x, y, z);
}

void
vpgl_geo_camera::translate(double tx, double ty, double /*z*/)
{
  // use the scale values
  if (scale_tag_)
  {
    trans_matrix_[0][3] += tx * trans_matrix_[0][0];
    trans_matrix_[1][3] += ty * trans_matrix_[1][1]; // multiplying by -1.0 to get sy
  }
  else
  {
    std::cout << "Warning! Translation offset will only be computed correctly for lidar pixel spacing = 1 meter\n";
    trans_matrix_[0][3] += tx;
    trans_matrix_[1][3] -= ty;
  }
}

//: returns the corresponding geographical coordinates for a given pixel position (i,j)
//  The output global coord is wgs84
void
vpgl_geo_camera::img_to_global(const double i, const double j, double & lon, double & lat) const
{
  vnl_vector<double> v(4), res(4);
  if (scale_tag_)
  {
    v[0] = trans_matrix_[0][3] + i * trans_matrix_[0][0];
    v[1] = trans_matrix_[1][3] + j * trans_matrix_[1][1];
  }
  else
  {
    v[0] = trans_matrix_[0][3] + i;
    v[1] = trans_matrix_[1][3] - j;
  }
  v[2] = 0;
  v[3] = 1;
  if (is_utm_)
  {
    vpgl_utm utm;
    double elev = 0.0;
    bool south_flag = northing_ > 0;
    utm.transform(utm_zone_, v[0], v[1], v[2], lat, lon, elev, south_flag);
  }
  else
  {
    // lon = v[0]; lat = v[1]; elev = v[2];
    lon = v[0];
    lat = v[1];
  }
}

//: returns the corresponding pixel position for given geographical coordinates
//  The input global coord is wgs84
void
vpgl_geo_camera::global_to_img(const double lon, const double lat, const double gz, double & u, double & v) const
{
  vnl_vector<double> vec(4), res(4);
  double x1 = lon, y1 = lat, z1 = gz;
  if (is_utm_)
  {
    vpgl_utm utm;
    int utm_zone;
    utm.transform(lat, lon, x1, y1, utm_zone);
    // std::cout << "utm returned x1: " << x1 << " y1: " << y1 << std::endl;
    // z1 = 0;
  }
  vec[0] = x1;
  vec[1] = y1;
  vec[2] = z1;
  vec[3] = 1;

  // do we really need this, const does not allow this
  vnl_matrix<double> tm(trans_matrix_);
  tm[2][2] = 1;

  if (scale_tag_)
  {
    u = (vec[0] - trans_matrix_[0][3]) / trans_matrix_[0][0];
    v = (vec[1] - trans_matrix_[1][3]) / trans_matrix_[1][1];
  }
  else
  {
    vnl_matrix<double> trans_matrix_inv = vnl_inverse(tm);
    res = trans_matrix_inv * vec;
    u = res[0];
    v = res[1];
  }
}

//: returns the corresponding geographical coordinates for a given pixel position (i,j)
//  The output global coord is UTM: x east, y north
void
vpgl_geo_camera::img_to_global_utm(const double i, const double j, double & x, double & y) const
{
  vnl_vector<double> v(4), res(4);
  if (scale_tag_)
  {
    v[0] = trans_matrix_[0][3] + i * trans_matrix_[0][0];
    v[1] = trans_matrix_[1][3] + j * trans_matrix_[1][1];
  }
  else
  {
    v[0] = trans_matrix_[0][3] + i;
    v[1] = trans_matrix_[1][3] - j;
  }
  v[2] = 0;
  v[3] = 1;
  if (is_utm_)
  {
    x = v[0];
    y = v[1];
  }
  else
  { // the trans matrix was using lat,lon coord, transform output to utm
    vpgl_utm utm;
    int dummy_zone;
    utm.transform(v[0], v[1], x, y, dummy_zone);
  }
}

//: returns the corresponding pixel position for given geographical coordinates
//  The input global coord is UTM: x east, for y north
void
vpgl_geo_camera::global_utm_to_img(const double x, const double y, int zone, double elev, double & u, double & v) const
{
  vnl_vector<double> vec(4), res(4);
  if (is_utm_)
  {
    vec[0] = x;
    vec[1] = y;
    vec[2] = elev;
  }
  else
  {
    vpgl_utm utm;
    double lat, lon, z;
    utm.transform(zone, x, y, elev, lat, lon, z);
    vec[0] = lat;
    vec[1] = lon;
    vec[2] = z;
  }
  vec[3] = 1;

  // do we really need this, const does not allow this
  vnl_matrix<double> tm(trans_matrix_);
  tm[2][2] = 1;

  if (scale_tag_)
  {
    u = (vec[0] - trans_matrix_[0][3]) / trans_matrix_[0][0];
    v = (vec[1] - trans_matrix_[1][3]) / trans_matrix_[1][1];
  }
  else
  {
    vnl_matrix<double> trans_matrix_inv = vnl_inverse(tm);
    res = trans_matrix_inv * vec;
    u = res[0];
    v = res[1];
  }
}

//: returns the corresponding utm location for the given local position
void
vpgl_geo_camera::local_to_utm(const double x, const double y, const double z, double & e, double & n, int & utm_zone)
  const
{
  double lat, lon, gz;
  lvcs_->local_to_global(x, y, z, vpgl_lvcs::wgs84, lon, lat, gz);

  vpgl_utm utm;
  utm.transform(lat, lon, e, n, utm_zone);
}

// save the camera matrix into a tfw file
void
vpgl_geo_camera::save_as_tfw(const std::string & tfw_filename)
{
  std::ofstream ofs(tfw_filename.c_str());
  ofs.precision(12);
  ofs << trans_matrix_[0][0] << '\n';
  ofs << trans_matrix_[0][1] << '\n';
  ofs << trans_matrix_[1][0] << '\n';
  ofs << trans_matrix_[1][1] << '\n';
  ofs << trans_matrix_[0][3] << '\n';
  ofs << trans_matrix_[1][3] << '\n';
  ofs.close();
}

bool
vpgl_geo_camera::img_four_corners_in_utm(const unsigned ni,
                                         const unsigned nj,
                                         double /*elev*/,
                                         double & e1,
                                         double & n1,
                                         double & e2,
                                         double & n2) const
{
  if (!is_utm_)
  {
    std::cerr << "In vpgl_geo_camera::img_four_corners_in_utm() -- UTM hasn't been set!\n";
    return false;
  }
  double lon, lat;
  this->img_to_global(0, 0, lon, lat);
  vpgl_utm utm;
  int utm_zone;
  utm.transform(lat, lon, e1, n1, utm_zone);
  this->img_to_global(ni, nj, lon, lat);
  utm.transform(lat, lon, e2, n2, utm_zone);
  return true;
}


bool
vpgl_geo_camera::operator==(const vpgl_geo_camera & rhs) const
{
  return this->trans_matrix_ == rhs.trans_matrix_ && *(this->lvcs_) == *(rhs.lvcs_);
}

//: Write vpgl_geo_camera to stream
std::ostream &
operator<<(std::ostream & s, const vpgl_geo_camera & p)
{
  if (p.lvcs_)
    s << p.trans_matrix_ << '\n' << *(p.lvcs_) << '\n';
  else
    s << p.trans_matrix_ << '\n';

  if (!p.is_utm_)
  {
    s << "geocam is using wgs84 deg/meters\n";
  }
  else
  {
    s << "geocam is using UTM with zone " << p.utm_zone_ << '\n';
    if (p.northing_)
    {
      s << "southern zone\n";
    }
    else
    {
      s << "northern zone\n";
    }
  }

  return s;
}

//: Read vpgl_perspective_camera from stream
std::istream &
operator>>(std::istream & s, vpgl_geo_camera & p)
{
  vnl_matrix_fixed<double, 4, 4> tr_matrix;
  s >> tr_matrix;
  vpgl_lvcs_sptr lvcs = new vpgl_lvcs();
  s >> (*lvcs);
  p = vpgl_geo_camera(tr_matrix.as_ref(), lvcs);
  return s;
}

bool
vpgl_geo_camera::comp_trans_matrix(double sx1,
                                   double sy1,
                                   double sz1,
                                   std::vector<std::vector<double>> tiepoints,
                                   vnl_matrix<double> & trans_matrix,
                                   bool scale_tag)
{
  // use tiepoints and scale values to create a transformation matrix
  // for now use the first tiepoint if there are more than one
  assert(tiepoints.size() > 0);
  assert(tiepoints[0].size() == 6);
  double I = tiepoints[0][0];
  double J = tiepoints[0][1];
  double K = tiepoints[0][2];
  double X = tiepoints[0][3];
  double Y = tiepoints[0][4];
  double Z = tiepoints[0][5];

  // Define a transformation matrix as follows:
  //
  //      |-                         -|
  //      |   Sx    0.0   0.0   Tx    |
  //      |                           |      Tx = X - I*Sx
  //      |   0.0  -Sy    0.0   Ty    |      Ty = Y + J*Sy
  //      |                           |      Tz = Z - K*Sz
  //      |   0.0   0.0   Sz    Tz    |
  //      |                           |
  //      |   0.0   0.0   0.0   1.0   |
  //      |-                         -|
  double sx = 1.0, sy = 1.0, sz = 1.0;
  if (scale_tag)
  {
    sx = sx1;
    sy = sy1;
    sz = sz1;
  }
  double Tx = X - I * sx;
  double Ty = Y + J * sy;
  double Tz = Z - K * sz;

  vnl_matrix<double> m(4, 4);
  m.fill(0.0);
  m[0][0] = sx;
  m[1][1] = -1 * sy;
  m[2][2] = sz;
  m[3][3] = 1.0;
  m[0][3] = Tx;
  m[1][3] = Ty;
  m[2][3] = Tz;
  trans_matrix = m;
  // std::cout << trans_matrix << std::endl;
  return true;
}

void
vpgl_geo_camera::img_to_wgs(unsigned /*i*/,
                            unsigned /*j*/,
                            unsigned /*k*/,
                            double & /*lon*/,
                            double & /*lat*/,
                            double & /*elev*/) const
{
  assert(!"Not yet implemented");
}

//: Binary save self to stream.
void
vpgl_geo_camera::b_write(vsl_b_ostream & os) const
{
  vsl_b_write(os, version());
  vsl_b_write(os, trans_matrix_.rows());
  vsl_b_write(os, trans_matrix_.cols());
  for (unsigned i = 0; i < trans_matrix_.rows(); i++)
    for (unsigned j = 0; j < trans_matrix_.cols(); j++)
      vsl_b_write(os, trans_matrix_[i][j]);

  lvcs_->b_write(os);
  vsl_b_write(os, is_utm_);
  vsl_b_write(os, utm_zone_);
  vsl_b_write(os, northing_);
  vsl_b_write(os, scale_tag_);
}


//: Binary load self from stream.
void
vpgl_geo_camera::b_read(vsl_b_istream & is)
{
  if (!is)
    return;
  short ver;
  vsl_b_read(is, ver);
  switch (ver)
  {
    case 1:
    {
      unsigned nrows, ncols;
      vsl_b_read(is, nrows);
      vsl_b_read(is, ncols);
      trans_matrix_.set_size(nrows, ncols);
      for (unsigned i = 0; i < nrows; i++)
        for (unsigned j = 0; j < ncols; j++)
          vsl_b_read(is, trans_matrix_[i][j]);

      vpgl_lvcs_sptr lvcs_ = new vpgl_lvcs(0, 0, 0);
      lvcs_->b_read(is);
      vsl_b_read(is, is_utm_);
      vsl_b_read(is, utm_zone_);
      vsl_b_read(is, northing_);
      vsl_b_read(is, scale_tag_);
      break;
    }

    default:
      std::cerr << "I/O ERROR: vpgl_geo_camera::b_read(vsl_b_istream&)\n"
                << "           Unknown version number " << ver << '\n';
      is.is().clear(std::ios::badbit); // Set an unrecoverable IO error on stream
      return;
  }
}

#if HAS_GEOTIFF

//: Create a vpgl_geo_camera from a geotiff file
vpgl_geo_camera
load_geo_camera_from_geotiff(const std::string & file, const vpgl_lvcs * lvcs)
{
  vpgl_geo_camera camera;
  if (!camera.load_from_geotiff(file, lvcs))
    throw std::runtime_error("Failed to load vpgl_geo_camera from geotiff file");
  return camera;
}

//: Create a vpgl_geo_camera from a vil_image_resource_sptr & optional LVCS
vpgl_geo_camera
load_geo_camera_from_resource(const vil_image_resource_sptr & geotiff_img, const vpgl_lvcs * lvcs)
{
  vpgl_geo_camera camera;
  if (!camera.load_from_resource(geotiff_img, lvcs))
    throw std::runtime_error("Failed to load vpgl_geo_camera");
  return camera;
}

#endif // HAS_GEOTIFF

//: Create a vpgl_geo_camera from GDAL geotransform
vpgl_geo_camera
load_geo_camera_from_geotransform(std::array<double, 6> geotransform,
                                  int utm_zone,
                                  int northing,
                                  const vpgl_lvcs * lvcs)
{
  vpgl_geo_camera camera;
  if (!camera.load_from_geotransform(geotransform, utm_zone, northing, lvcs))
    throw std::runtime_error("Failed to load vpgl_geo_camera from geotransform");
  return camera;
}
