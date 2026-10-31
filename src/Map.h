#pragma once

#include "raylib.h"
#include <vector>

// ------------------------------------------------------------
// The match arena.
// A map is just a list of boxes (walls, crates, containers), the spots
// where each team can spawn, and a grid of points the bots walk between.
// ------------------------------------------------------------

struct MapBox {
    BoundingBox bounds;   // the box's min and max corners
    Color color;
};

// A point bots can stand on, and the points next to it they can walk to
struct Waypoint {
    Vector3 position;
    std::vector<int> links;
};

struct Map {
    const char* name = "";
    float halfWidth = 0;     // the floor goes from -halfWidth to +halfWidth on x
    float halfDepth = 0;     // ... and from -halfDepth to +halfDepth on z
    std::vector<MapBox> boxes;
    std::vector<Vector3> spawns[2];   // spawn points for team 0 (you) and team 1
    std::vector<Waypoint> waypoints;

    void buildKilnYard();             // makes the map (see Map.cpp)
    void draw(Vector3 eye) const;     // eye = camera position, for the distance haze

    // Does a box touch any wall? (touching the top of a box you stand on doesn't count)
    bool overlaps(BoundingBox box) const;
    // The highest top of the walls a box overlaps (where you land when you fall onto them)
    float highestTop(BoundingBox box) const;
    // How far a ray goes before it hits a wall. Returns false if it hits nothing within maxDistance.
    bool rayHit(Ray ray, float maxDistance, float& hitDistance) const;
    // True if nothing blocks the straight line between two points
    bool canSee(Vector3 from, Vector3 to) const;
    // True if a soldier could walk in a straight line between two points on the floor
    bool canWalk(Vector3 from, Vector3 to) const;

    int nearestWaypoint(Vector3 position) const;
    // The waypoints to walk through to get from one waypoint to another (empty if there's no way)
    std::vector<int> findPath(int from, int to) const;

private:
    void addBox(Vector3 center, Vector3 size, Color color);
    void addMirrored(Vector3 center, Vector3 size, Color color);
    void addSpawn(int team, float x, float z);
    void buildWaypoints();
};
