#include "Map.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <queue>

// Map colors
static const Color BRICK      = { 150, 84, 62, 255 };
static const Color BRICK_DARK = { 112, 64, 50, 255 };
static const Color CONCRETE   = { 132, 130, 122, 255 };
static const Color ROOF       = { 74, 68, 64, 255 };
static const Color WOOD       = { 156, 118, 72, 255 };
static const Color RUST       = { 156, 82, 48, 255 };
static const Color TEAL       = { 52, 104, 108, 255 };
static const Color NAVY       = { 58, 70, 102, 255 };
static const Color MOSS       = { 92, 104, 62, 255 };
static const Color FLOOR_A    = { 98, 96, 90, 255 };
static const Color FLOOR_B    = { 91, 89, 83, 255 };
static const Color HAZE       = { 190, 170, 148, 255 };   // far away things fade toward this

// How big a soldier is for walking checks (half its width, and its height)
static const float BODY_RADIUS = 0.36f;
static const float BODY_HEIGHT = 1.7f;

// ------------------------------------------------------------
// Building the map
// ------------------------------------------------------------

// Adds a box. Long walls get cut into pieces of about 6 meters, so the haze
// (which is worked out at each corner) looks right on them.
void Map::addBox(Vector3 center, Vector3 size, Color color) {
    int piecesX = (int)ceilf(size.x / 6);
    int piecesZ = (int)ceilf(size.z / 6);
    Vector3 piece = { size.x / piecesX, size.y, size.z / piecesZ };
    for (int i = 0; i < piecesX; i++) {
        for (int j = 0; j < piecesZ; j++) {
            float x = center.x - size.x / 2 + piece.x * (i + 0.5f);
            float z = center.z - size.z / 2 + piece.z * (j + 0.5f);
            MapBox box;
            box.bounds.min = { x - piece.x / 2, center.y - size.y / 2, z - piece.z / 2 };
            box.bounds.max = { x + piece.x / 2, center.y + size.y / 2, z + piece.z / 2 };
            box.color = color;
            boxes.push_back(box);
        }
    }
}

// The map is the same on both sides: every box on one side gets a twin
// on the other side, turned around the middle of the map (x and z flipped).
// That way neither team gets a better half.
void Map::addMirrored(Vector3 center, Vector3 size, Color color) {
    addBox(center, size, color);
    addBox({ -center.x, center.y, -center.z }, size, color);
}

// Team 0 spawns on the west side (negative x), team 1 at the mirrored spot in the east
void Map::addSpawn(int team, float x, float z) {
    spawns[team].push_back({ x, 0, z });
    spawns[1 - team].push_back({ -x, 0, -z });
}

void Map::buildKilnYard() {
    name = "KILN YARD";
    halfWidth = 36;
    halfDepth = 24;
    boxes.clear();
    spawns[0].clear();
    spawns[1].clear();

    // Outer walls (north + south, then west + east)
    addMirrored({ 0, 2.5f, 24 }, { 73, 5, 1 }, BRICK_DARK);
    addMirrored({ -36, 2.5f, 0 }, { 1, 5, 47 }, BRICK_DARK);

    // ---- The kiln: a brick building in the middle with doors on every side ----
    // West wall (the mirror makes the east wall), with a door in the middle
    addMirrored({ -6.7f, 2, -3.1f }, { 0.6f, 4, 3.8f }, BRICK);
    addMirrored({ -6.7f, 2, 3.1f }, { 0.6f, 4, 3.8f }, BRICK);
    addMirrored({ -6.7f, 3.3f, 0 }, { 0.6f, 1.4f, 2.4f }, BRICK);        // above the door
    // North wall (the mirror makes the south wall), with two doors
    addMirrored({ -5.75f, 2, 4.7f }, { 2.5f, 4, 0.6f }, BRICK);
    addMirrored({ 0, 2, 4.7f }, { 5, 4, 0.6f }, BRICK);
    addMirrored({ 5.75f, 2, 4.7f }, { 2.5f, 4, 0.6f }, BRICK);
    addMirrored({ -3.5f, 3.3f, 4.7f }, { 2, 1.4f, 0.6f }, BRICK);         // above the doors
    addMirrored({ 3.5f, 3.3f, 4.7f }, { 2, 1.4f, 0.6f }, BRICK);
    addBox({ 0, 4.2f, 0 }, { 14.6f, 0.4f, 10.6f }, ROOF);
    addBox({ 0, 1.25f, 0 }, { 3, 2.5f, 2.6f }, BRICK_DARK);              // the kiln itself
    addBox({ 0, 5.5f, 0 }, { 1.4f, 3, 1.4f }, BRICK_DARK);               // its chimney
    addMirrored({ -4.8f, 0.6f, -2.8f }, { 1.2f, 1.2f, 1.2f }, WOOD);     // crates inside

    // ---- North-west lane (mirrored into the south-east lane) ----
    addMirrored({ -17, 1.3f, 15 }, { 6.5f, 2.6f, 2.5f }, RUST);          // shipping container
    addMirrored({ -16.5f, 3.9f, 15 }, { 6, 2.6f, 2.5f }, TEAL);          // another one stacked on top
    addMirrored({ -9, 1.3f, 18.5f }, { 2.5f, 2.6f, 6 }, NAVY);
    addMirrored({ -13, 0.6f, 11.5f }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -3.5f, 0.6f, 16 }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -3.5f, 1.8f, 16 }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -2.5f, 0.6f, 17.2f }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -4, 0.55f, 10.5f }, { 5, 1.1f, 0.5f }, CONCRETE);      // low wall

    // ---- South-west lane (mirrored into the north-east lane) ----
    addMirrored({ -20, 1.3f, -15 }, { 6.5f, 2.6f, 2.5f }, MOSS);
    addMirrored({ -11, 1.3f, -19.5f }, { 6.5f, 2.6f, 2.5f }, RUST);
    addMirrored({ -5, 1.3f, -14 }, { 2.5f, 2.6f, 6.5f }, TEAL);
    addMirrored({ -15, 0.6f, -10 }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -26, 0.6f, -19 }, { 1.2f, 1.2f, 1.2f }, WOOD);

    // ---- Between the west spawn and the kiln (mirrored to the east) ----
    addMirrored({ -13, 0.55f, 5 }, { 0.5f, 1.1f, 4 }, CONCRETE);
    addMirrored({ -13, 0.55f, -5 }, { 0.5f, 1.1f, 4 }, CONCRETE);
    addMirrored({ -20, 1.5f, 0 }, { 0.6f, 3, 6 }, BRICK);                // blocks the view into spawn
    addMirrored({ -22, 0.6f, 8 }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -21.6f, 0.6f, -7.5f }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -27, 0.6f, 5 }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -27, 1.8f, 5 }, { 1.2f, 1.2f, 1.2f }, WOOD);
    addMirrored({ -27, 0.6f, -5 }, { 1.2f, 1.2f, 1.2f }, WOOD);

    // Spawn points (each one also makes a mirrored spawn for the other team)
    addSpawn(0, -32, -12);
    addSpawn(0, -32, -6);
    addSpawn(0, -32, 0);
    addSpawn(0, -32, 6);
    addSpawn(0, -32, 12);
    addSpawn(0, -29, -19);
    addSpawn(0, -29, 19);
    addSpawn(0, -29, 0);

    buildWaypoints();
}

// Bots find their way using a grid of points 2 meters apart.
// Points inside walls are thrown away, and each point is linked to the
// neighbors you can walk to in a straight line. If you change the map,
// this updates by itself.
void Map::buildWaypoints() {
    waypoints.clear();
    const float spacing = 2;
    int columns = (int)(halfWidth * 2 / spacing);
    int rows = (int)(halfDepth * 2 / spacing);
    std::vector<int> grid(columns * rows, -1);   // grid cell -> waypoint number (-1 = blocked)

    for (int c = 0; c < columns; c++) {
        for (int r = 0; r < rows; r++) {
            Vector3 p = { -halfWidth + spacing + c * spacing, 0, -halfDepth + spacing + r * spacing };
            BoundingBox body = { { p.x - 0.5f, 0.1f, p.z - 0.5f }, { p.x + 0.5f, BODY_HEIGHT, p.z + 0.5f } };
            if (overlaps(body)) continue;
            grid[c * rows + r] = (int)waypoints.size();
            waypoints.push_back({ p, {} });
        }
    }

    // Link each point to its 8 neighbors (if nothing is in the way)
    for (int c = 0; c < columns; c++) {
        for (int r = 0; r < rows; r++) {
            int a = grid[c * rows + r];
            if (a < 0) continue;
            for (int dc = -1; dc <= 1; dc++) {
                for (int dr = -1; dr <= 1; dr++) {
                    int nc = c + dc, nr = r + dr;
                    if ((dc == 0 && dr == 0) || nc < 0 || nr < 0 || nc >= columns || nr >= rows) continue;
                    int b = grid[nc * rows + nr];
                    if (b >= 0 && canWalk(waypoints[a].position, waypoints[b].position)) {
                        waypoints[a].links.push_back(b);
                    }
                }
            }
        }
    }
    TraceLog(LOG_INFO, "MAP: %s has %d boxes and %d bot waypoints", name, (int)boxes.size(), (int)waypoints.size());
}

// ------------------------------------------------------------
// Collision
// ------------------------------------------------------------

// Boxes that only touch (like your feet on top of a crate) don't count as overlapping
static bool boxesOverlap(const BoundingBox& a, const BoundingBox& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

bool Map::overlaps(BoundingBox box) const {
    for (const MapBox& wall : boxes) {
        if (boxesOverlap(box, wall.bounds)) return true;
    }
    return false;
}

float Map::highestTop(BoundingBox box) const {
    float top = 0;
    for (const MapBox& wall : boxes) {
        if (boxesOverlap(box, wall.bounds) && wall.bounds.max.y > top) top = wall.bounds.max.y;
    }
    return top;
}

bool Map::rayHit(Ray ray, float maxDistance, float& hitDistance) const {
    float best = maxDistance;
    bool hit = false;
    for (const MapBox& wall : boxes) {
        RayCollision c = GetRayCollisionBox(ray, wall.bounds);
        if (c.hit && c.distance < best) {
            best = c.distance;
            hit = true;
        }
    }
    // The floor (y = 0)
    if (ray.direction.y < 0) {
        float t = -ray.position.y / ray.direction.y;
        if (t >= 0 && t < best) {
            best = t;
            hit = true;
        }
    }
    hitDistance = best;
    return hit;
}

bool Map::canSee(Vector3 from, Vector3 to) const {
    Vector3 difference = Vector3Subtract(to, from);
    float distance = Vector3Length(difference);
    if (distance < 0.001f) return true;
    Ray ray = { from, Vector3Scale(difference, 1 / distance) };
    float hitDistance;
    return !rayHit(ray, distance, hitDistance);
}

bool Map::canWalk(Vector3 from, Vector3 to) const {
    // Check a soldier-sized box every 0.4 meters along the line
    float distance = Vector3Distance(from, to);
    int steps = (int)(distance / 0.4f) + 1;
    for (int i = 0; i <= steps; i++) {
        Vector3 p = Vector3Lerp(from, to, (float)i / steps);
        BoundingBox body = { { p.x - BODY_RADIUS, 0.1f, p.z - BODY_RADIUS }, { p.x + BODY_RADIUS, BODY_HEIGHT, p.z + BODY_RADIUS } };
        if (overlaps(body)) return false;
    }
    return true;
}

// ------------------------------------------------------------
// Finding a path for the bots
// ------------------------------------------------------------

int Map::nearestWaypoint(Vector3 position) const {
    // Sort the closest few by distance, and take the first one we can walk straight to
    std::vector<std::pair<float, int>> closest;
    for (int i = 0; i < (int)waypoints.size(); i++) {
        float dx = waypoints[i].position.x - position.x;
        float dz = waypoints[i].position.z - position.z;
        float d = dx * dx + dz * dz;
        if (d < 36) closest.push_back({ d, i });   // within 6 meters
    }
    std::sort(closest.begin(), closest.end());
    Vector3 feet = { position.x, 0, position.z };
    for (int i = 0; i < (int)closest.size() && i < 6; i++) {
        if (canWalk(feet, waypoints[closest[i].second].position)) return closest[i].second;
    }
    if (!closest.empty()) return closest[0].second;

    // Nothing close: just take the nearest one anywhere
    int best = 0;
    float bestDistance = 1e9f;
    for (int i = 0; i < (int)waypoints.size(); i++) {
        float d = Vector3DistanceSqr(waypoints[i].position, feet);
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

// Breadth-first search: spread out from the start one link at a time,
// remembering where we came from, until we reach the goal.
std::vector<int> Map::findPath(int from, int to) const {
    std::vector<int> cameFrom(waypoints.size(), -1);
    std::queue<int> open;
    open.push(from);
    cameFrom[from] = from;
    while (!open.empty()) {
        int current = open.front();
        open.pop();
        if (current == to) break;
        for (int next : waypoints[current].links) {
            if (cameFrom[next] == -1) {
                cameFrom[next] = current;
                open.push(next);
            }
        }
    }
    std::vector<int> path;
    if (cameFrom[to] == -1) return path;   // can't get there
    for (int at = to; at != from; at = cameFrom[at]) path.push_back(at);
    path.push_back(from);
    std::reverse(path.begin(), path.end());
    return path;
}

// ------------------------------------------------------------
// Drawing
// There's no real lighting: each side of a box just gets a different
// brightness (as if the sun were up and to one side), the bottom of each
// wall is a bit darker, and things far away fade into the haze.
// ------------------------------------------------------------

static Vector3 mapEye;

static void vertex(float x, float y, float z, Color color, float brightness) {
    float distance = sqrtf((x - mapEye.x) * (x - mapEye.x) + (z - mapEye.z) * (z - mapEye.z));
    float haze = Clamp((distance - 18) / 70, 0, 0.55f);
    rlColor4ub((unsigned char)Lerp(color.r * brightness, HAZE.r, haze),
               (unsigned char)Lerp(color.g * brightness, HAZE.g, haze),
               (unsigned char)Lerp(color.b * brightness, HAZE.b, haze), 255);
    rlVertex3f(x, y, z);
}

// One side of a box: corners go counter-clockwise when you look at the side from outside
// (the graphics card skips sides facing away from you). a and b are the bottom corners.
static void side(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color, float brightness, bool shadeBottom) {
    float bottom = shadeBottom ? brightness * 0.72f : brightness;
    vertex(a.x, a.y, a.z, color, bottom);
    vertex(b.x, b.y, b.z, color, bottom);
    vertex(c.x, c.y, c.z, color, brightness);
    vertex(d.x, d.y, d.z, color, brightness);
}

static void drawBox(const BoundingBox& b, Color color) {
    float x0 = b.min.x, y0 = b.min.y, z0 = b.min.z;
    float x1 = b.max.x, y1 = b.max.y, z1 = b.max.z;
    bool onFloor = y0 < 0.01f;
    rlCheckRenderBatchLimit(24);
    rlBegin(RL_QUADS);
    side({ x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, color, 0.80f, onFloor);   // +z
    side({ x1, y0, z0 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, color, 0.88f, onFloor);   // -z
    side({ x1, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, color, 0.70f, onFloor);   // +x
    side({ x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, color, 0.62f, onFloor);   // -x
    side({ x0, y1, z0 }, { x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 }, color, 1.0f, false);      // top
    side({ x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 }, color, 0.5f, false);      // bottom
    rlEnd();
}

void Map::draw(Vector3 eye) const {
    mapEye = eye;

    // Floor: a checkerboard of 4 meter concrete slabs
    const float tile = 4;
    for (float x = -halfWidth; x < halfWidth; x += tile) {
        for (float z = -halfDepth; z < halfDepth; z += tile) {
            bool even = ((int)((x + halfWidth) / tile) + (int)((z + halfDepth) / tile)) % 2 == 0;
            Color c = even ? FLOOR_A : FLOOR_B;
            rlCheckRenderBatchLimit(4);
            rlBegin(RL_QUADS);
            vertex(x, 0, z, c, 1);
            vertex(x, 0, z + tile, c, 1);
            vertex(x + tile, 0, z + tile, c, 1);
            vertex(x + tile, 0, z, c, 1);
            rlEnd();
        }
    }

    for (const MapBox& box : boxes) {
        drawBox(box.bounds, box.color);
    }
}
