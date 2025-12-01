#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <memory>
#include <algorithm>
#include <limits>
#include <fstream>
  
// ============================================================================
// MATH UTILITIES
// ============================================================================

struct Vec3 {
    double x, y, z;
    
    Vec3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    
    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(double t) const { return Vec3(x * t, y * t, z * t); }
    Vec3 operator*(const Vec3& v) const { return Vec3(x * v.x, y * v.y, z * v.z); }
    Vec3 operator/(double t) const { return Vec3(x / t, y / t, z / t); }
    
    double dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const {
        return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }
    
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double lengthSquared() const { return x * x + y * y + z * z; }
    Vec3 normalize() const { return *this / length(); }
};

struct Ray {
    Vec3 origin, direction;
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d.normalize()) {}
};

struct AABB {
    Vec3 min, max;
    
    AABB() : min(Vec3(1e10, 1e10, 1e10)), max(Vec3(-1e10, -1e10, -1e10)) {}
    AABB(const Vec3& min, const Vec3& max) : min(min), max(max) {}
    
    void expand(const Vec3& p) {
        min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y); min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y); max.z = std::max(max.z, p.z);
    }
    
    void expand(const AABB& box) {
        expand(box.min);
        expand(box.max);
    }
    
    Vec3 centroid() const { return (min + max) * 0.5; }
    
    bool intersect(const Ray& ray, double tMin, double tMax) const {
        for (int a = 0; a < 3; a++) {
            double invD = 1.0 / (&ray.direction.x)[a];
            double t0 = ((&min.x)[a] - (&ray.origin.x)[a]) * invD;
            double t1 = ((&max.x)[a] - (&ray.origin.x)[a]) * invD;
            if (invD < 0.0) std::swap(t0, t1);
            tMin = std::max(t0, tMin);
            tMax = std::min(t1, tMax);
            if (tMax <= tMin) return false;
        }
        return true;
    }
};

// ============================================================================
// RANDOM NUMBER GENERATION (Monte Carlo sampling)
// ============================================================================

class Random {
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist;
public:
    Random() : gen(std::random_device{}()), dist(0.0, 1.0) {}
    double uniform() { return dist(gen); }
    
    Vec3 uniformHemisphere(const Vec3& normal) {
        double r1 = uniform(), r2 = uniform();
        double theta = acos(r1);
        double phi = 2 * M_PI * r2;
        
        Vec3 tangent = std::abs(normal.x) > 0.1 ? 
            Vec3(0, 1, 0).cross(normal).normalize() : 
            Vec3(1, 0, 0).cross(normal).normalize();
        Vec3 bitangent = normal.cross(tangent);
        
        return (tangent * cos(phi) * sin(theta) + 
                bitangent * sin(phi) * sin(theta) + 
                normal * cos(theta)).normalize();
    }
    
    Vec3 cosineWeightedHemisphere(const Vec3& normal) {
        double r1 = uniform(), r2 = uniform();
        double theta = asin(sqrt(r1));
        double phi = 2 * M_PI * r2;
        
        Vec3 tangent = std::abs(normal.x) > 0.1 ? 
            Vec3(0, 1, 0).cross(normal).normalize() : 
            Vec3(1, 0, 0).cross(normal).normalize();
        Vec3 bitangent = normal.cross(tangent);
        
        return (tangent * cos(phi) * sin(theta) + 
                bitangent * sin(phi) * sin(theta) + 
                normal * cos(theta)).normalize();
    }
};

// ============================================================================
// MATERIALS
// ============================================================================

struct HitRecord;

enum MaterialType { DIFFUSE, METAL, DIELECTRIC, EMISSIVE, GLASS };

struct Material {
    MaterialType type;
    Vec3 albedo;
    Vec3 emission;
    double roughness;
    double ior; // Index of refraction
    
    Material(MaterialType t = DIFFUSE, Vec3 a = Vec3(0.8, 0.8, 0.8), 
             Vec3 e = Vec3(0, 0, 0), double r = 0.5, double i = 1.5)
        : type(t), albedo(a), emission(e), roughness(r), ior(i) {}
    
    bool scatter(const Ray& rayIn, const HitRecord& rec, Vec3& attenuation, 
                 Ray& scattered, Random& rng) const;
};

// ============================================================================
// GEOMETRY
// ============================================================================

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    double t;
    const Material* material;
    bool frontFace;
    
    void setFaceNormal(const Ray& ray, const Vec3& outwardNormal) {
        frontFace = ray.direction.dot(outwardNormal) < 0;
        normal = frontFace ? outwardNormal : outwardNormal * -1;
    }
};

class Hittable {
public:
    virtual ~Hittable() = default;
    virtual bool hit(const Ray& ray, double tMin, double tMax, HitRecord& rec) const = 0;
    virtual AABB boundingBox() const = 0;
};

class Sphere : public Hittable {
    Vec3 center;
    double radius;
    Material material;
    
public:
    Sphere(const Vec3& c, double r, const Material& m) 
        : center(c), radius(r), material(m) {}
    
    bool hit(const Ray& ray, double tMin, double tMax, HitRecord& rec) const override {
        Vec3 oc = ray.origin - center;
        double a = ray.direction.lengthSquared();
        double halfB = oc.dot(ray.direction);
        double c = oc.lengthSquared() - radius * radius;
        double discriminant = halfB * halfB - a * c;
        
        if (discriminant < 0) return false;
        
        double sqrtd = std::sqrt(discriminant);
        double root = (-halfB - sqrtd) / a;
        if (root < tMin || root > tMax) {
            root = (-halfB + sqrtd) / a;
            if (root < tMin || root > tMax) return false;
        }
        
        rec.t = root;
        rec.point = ray.origin + ray.direction * rec.t;
        Vec3 outwardNormal = (rec.point - center) / radius;
        rec.setFaceNormal(ray, outwardNormal);
        rec.material = &material;
        return true;
    }
    
    AABB boundingBox() const override {
        Vec3 r(radius, radius, radius);
        return AABB(center - r, center + r);
    }
};

class Plane : public Hittable {
    Vec3 point, normal;
    Material material;
    
public:
    Plane(const Vec3& p, const Vec3& n, const Material& m)
        : point(p), normal(n.normalize()), material(m) {}
    
    bool hit(const Ray& ray, double tMin, double tMax, HitRecord& rec) const override {
        double denom = normal.dot(ray.direction);
        if (std::abs(denom) < 1e-8) return false;
        
        double t = (point - ray.origin).dot(normal) / denom;
        if (t < tMin || t > tMax) return false;
        
        rec.t = t;
        rec.point = ray.origin + ray.direction * t;
        rec.setFaceNormal(ray, normal);
        rec.material = &material;
        return true;
    }
    
    AABB boundingBox() const override {
        return AABB(Vec3(-1e10, -1e10, -1e10), Vec3(1e10, 1e10, 1e10));
    }
};

// ============================================================================
// BVH ACCELERATION STRUCTURE
// ============================================================================

class BVHNode : public Hittable {
    std::shared_ptr<Hittable> left, right;
    AABB box;
    
public:
    BVHNode(std::vector<std::shared_ptr<Hittable>>& objects, size_t start, size_t end) {
        int axis = rand() % 3;
        auto comparator = [axis](const std::shared_ptr<Hittable>& a, 
                                 const std::shared_ptr<Hittable>& b) {
            AABB boxA = a->boundingBox();
            AABB boxB = b->boundingBox();
            return (&boxA.min.x)[axis] < (&boxB.min.x)[axis];
        };
        
        size_t span = end - start;
        if (span == 1) {
            left = right = objects[start];
        } else if (span == 2) {
            if (comparator(objects[start], objects[start + 1])) {
                left = objects[start];
                right = objects[start + 1];
            } else {
                left = objects[start + 1];
                right = objects[start];
            }
        } else {
            std::sort(objects.begin() + start, objects.begin() + end, comparator);
            size_t mid = start + span / 2;
            left = std::make_shared<BVHNode>(objects, start, mid);
            right = std::make_shared<BVHNode>(objects, mid, end);
        }
        
        box = left->boundingBox();
        box.expand(right->boundingBox());
    }
    
    bool hit(const Ray& ray, double tMin, double tMax, HitRecord& rec) const override {
        if (!box.intersect(ray, tMin, tMax)) return false;
        
        bool hitLeft = left->hit(ray, tMin, tMax, rec);
        bool hitRight = right->hit(ray, tMin, hitLeft ? rec.t : tMax, rec);
        
        return hitLeft || hitRight;
    }
    
    AABB boundingBox() const override { return box; }
};

// ============================================================================
// MATERIAL SCATTERING IMPLEMENTATION
// ============================================================================

Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - n * 2 * v.dot(n);
}

Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    double cosTheta = std::min(uv * -1.0).dot(n), 1.0);
    Vec3 rOutPerp = (uv + n * cosTheta) * etai_over_etat;
    Vec3 rOutParallel = n * -std::sqrt(std::abs(1.0 - rOutPerp.lengthSquared()));
    return rOutPerp + rOutParallel;
}

double schlick(double cosine, double ior) {
    double r0 = (1 - ior) / (1 + ior);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}

bool Material::scatter(const Ray& rayIn, const HitRecord& rec, Vec3& attenuation, 
                       Ray& scattered, Random& rng) const {
    switch (type) {
        case DIFFUSE: {
            Vec3 scatterDir = rec.normal + rng.cosineWeightedHemisphere(rec.normal);
            if (scatterDir.lengthSquared() < 1e-8) scatterDir = rec.normal;
            scattered = Ray(rec.point, scatterDir);
            attenuation = albedo;
            return true;
        }
        case METAL: {
            Vec3 reflected = reflect(rayIn.direction, rec.normal);
            Vec3 fuzz = rng.uniformHemisphere(rec.normal) * roughness;
            scattered = Ray(rec.point, reflected + fuzz);
            attenuation = albedo;
            return scattered.direction.dot(rec.normal) > 0;
        }
        case DIELECTRIC:
        case GLASS: {
            attenuation = Vec3(1.0, 1.0, 1.0);
            double refractionRatio = rec.frontFace ? (1.0 / ior) : ior;
            
            Vec3 unitDir = rayIn.direction.normalize();
            double cosTheta = std::min((unitDir * -1.0).dot(rec.normal), 1.0);
            double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
            
            bool cannotRefract = refractionRatio * sinTheta > 1.0;
            Vec3 direction;
            
            if (cannotRefract || schlick(cosTheta, refractionRatio) > rng.uniform()) {
                direction = reflect(unitDir, rec.normal);
            } else {
                direction = refract(unitDir, rec.normal, refractionRatio);
            }
            
            scattered = Ray(rec.point, direction);
            return true;
        }
        case EMISSIVE:
            return false;
    }
    return false;
}

// ============================================================================
// CAMERA
// ============================================================================

class Camera {
    Vec3 origin, lowerLeft, horizontal, vertical;
    Vec3 u, v, w;
    double lensRadius;
    
public:
    Camera(Vec3 lookFrom, Vec3 lookAt, Vec3 vup, double vfov, 
           double aspect, double aperture, double focusDist) {
        double theta = vfov * M_PI / 180;
        double h = tan(theta / 2);
        double viewportHeight = 2.0 * h;
        double viewportWidth = aspect * viewportHeight;
        
        w = (lookFrom - lookAt).normalize();
        u = vup.cross(w).normalize();
        v = w.cross(u);
        
        origin = lookFrom;
        horizontal = u * viewportWidth * focusDist;
        vertical = v * viewportHeight * focusDist;
        lowerLeft = origin - horizontal / 2 - vertical / 2 - w * focusDist;
        lensRadius = aperture / 2;
    }
    
    Ray getRay(double s, double t) const {
        Vec3 rd = Vec3(0, 0, 0); // Simplified: no depth of field
        Vec3 offset = u * rd.x + v * rd.y;
        return Ray(origin + offset, 
                   lowerLeft + horizontal * s + vertical * t - origin - offset);
    }
};

// ============================================================================
// PATH TRACER
// ============================================================================

class PathTracer {
    std::shared_ptr<Hittable> world;
    Camera camera;
    Random rng;
    int maxDepth;
    
    Vec3 rayColor(const Ray& ray, int depth) {
        if (depth <= 0) return Vec3(0, 0, 0);
        
        HitRecord rec;
        if (world->hit(ray, 0.001, std::numeric_limits<double>::infinity(), rec)) {
            Vec3 emitted = rec.material->emission;
            Ray scattered;
            Vec3 attenuation;
            
            if (rec.material->scatter(ray, rec, attenuation, scattered, rng)) {
                return emitted + attenuation * rayColor(scattered, depth - 1);
            }
            return emitted;
        }
        
        // Sky gradient
        Vec3 unitDir = ray.direction.normalize();
        double t = 0.5 * (unitDir.y + 1.0);
        return Vec3(1.0, 1.0, 1.0) * (1.0 - t) + Vec3(0.5, 0.7, 1.0) * t;
    }
    
public:
    PathTracer(std::shared_ptr<Hittable> w, const Camera& c, int md = 10)
        : world(w), camera(c), maxDepth(md) {}
    
    void render(int width, int height, int samplesPerPixel, const std::string& filename) {
        std::vector<Vec3> image(width * height);
        
        std::cout << "Rendering " << width << "x" << height << " with " 
                  << samplesPerPixel << " samples...\n";
        
        for (int j = height - 1; j >= 0; --j) {
            std::cout << "\rScanlines remaining: " << j << ' ' << std::flush;
            for (int i = 0; i < width; ++i) {
                Vec3 color(0, 0, 0);
                for (int s = 0; s < samplesPerPixel; ++s) {
                    double u = (i + rng.uniform()) / (width - 1);
                    double v = (j + rng.uniform()) / (height - 1);
                    Ray ray = camera.getRay(u, v);
                    color = color + rayColor(ray, maxDepth);
                }
                color = color / samplesPerPixel;
                
                // Gamma correction
                color.x = std::sqrt(color.x);
                color.y = std::sqrt(color.y);
                color.z = std::sqrt(color.z);
                
                image[j * width + i] = color;
            }
        }
        std::cout << "\nDone!\n";
        
        // Write PPM file
        std::ofstream file(filename);
        file << "P3\n" << width << " " << height << "\n255\n";
        for (const auto& pixel : image) {
            int ir = static_cast<int>(255.99 * std::clamp(pixel.x, 0.0, 1.0));
            int ig = static_cast<int>(255.99 * std::clamp(pixel.y, 0.0, 1.0));
            int ib = static_cast<int>(255.99 * std::clamp(pixel.z, 0.0, 1.0));
            file << ir << " " << ig << " " << ib << "\n";
        }
    }
};

// ============================================================================
// MAIN - SCENE SETUP
// ============================================================================

int main() {
    // Image settings
    const double aspectRatio = 16.0 / 9.0;
    const int imageWidth = 800;
    const int imageHeight = static_cast<int>(imageWidth / aspectRatio);
    const int samplesPerPixel = 100;
    const int maxDepth = 50;
    
    // World
    std::vector<std::shared_ptr<Hittable>> objects;
    
    // Ground plane
    objects.push_back(std::make_shared<Plane>(
        Vec3(0, 0, 0), Vec3(0, 1, 0),
        Material(DIFFUSE, Vec3(0.5, 0.5, 0.5))));
    
    // Spheres with different materials
    objects.push_back(std::make_shared<Sphere>(
        Vec3(0, 1, 0), 1.0,
        Material(DIELECTRIC, Vec3(1.0, 1.0, 1.0), Vec3(0, 0, 0), 0.0, 1.5)));
    
    objects.push_back(std::make_shared<Sphere>(
        Vec3(-4, 1, 0), 1.0,
        Material(DIFFUSE, Vec3(0.4, 0.2, 0.1))));
    
    objects.push_back(std::make_shared<Sphere>(
        Vec3(4, 1, 0), 1.0,
        Material(METAL, Vec3(0.7, 0.6, 0.5), Vec3(0, 0, 0), 0.1)));
    
    // Light source
    objects.push_back(std::make_shared<Sphere>(
        Vec3(0, 6, -2), 1.5,
        Material(EMISSIVE, Vec3(1, 1, 1), Vec3(4, 4, 4))));
    
    // Random small spheres
    Random rng;
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            double chooseMat = rng.uniform();
            Vec3 center(a + 0.9 * rng.uniform(), 0.2, b + 0.9 * rng.uniform());
            
            if ((center - Vec3(4, 0.2, 0)).length() > 0.9) {
                if (chooseMat < 0.8) {
                    Vec3 albedo(rng.uniform() * rng.uniform(), 
                               rng.uniform() * rng.uniform(),
                               rng.uniform() * rng.uniform());
                    objects.push_back(std::make_shared<Sphere>(
                        center, 0.2, Material(DIFFUSE, albedo)));
                } else if (chooseMat < 0.95) {
                    Vec3 albedo(rng.uniform() * 0.5 + 0.5,
                               rng.uniform() * 0.5 + 0.5,
                               rng.uniform() * 0.5 + 0.5);
                    double fuzz = rng.uniform() * 0.5;
                    objects.push_back(std::make_shared<Sphere>(
                        center, 0.2, Material(METAL, albedo, Vec3(0, 0, 0), fuzz)));
                } else {
                    objects.push_back(std::make_shared<Sphere>(
                        center, 0.2, Material(GLASS, Vec3(1, 1, 1), Vec3(0, 0, 0), 0.0, 1.5)));
                }
            }
        }
    }
    
    // Build BVH
    auto world = std::make_shared<BVHNode>(objects, 0, objects.size());
    
    // Camera
    Vec3 lookFrom(13, 2, 3);
    Vec3 lookAt(0, 0, 0);
    Vec3 vup(0, 1, 0);
    double distToFocus = 10.0;
    double aperture = 0.1;
    
    Camera cam(lookFrom, lookAt, vup, 20, aspectRatio, aperture, distToFocus);
    
    // Render
    PathTracer tracer(world, cam, maxDepth);
    tracer.render(imageWidth, imageHeight, samplesPerPixel, "output.ppm");
    
    std::cout << "Image saved to output.ppm\n";
    return 0;

}
