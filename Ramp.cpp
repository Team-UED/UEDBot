#include "BasicSc2Bot.h"

// ------------------ Helper Functions ------------------
Point2D Point2D_mean(const std::vector<Point2D>& points)
{
	Point2D mean;

	for (const auto& p : points)
	{
		mean += p;
	}
	mean /= points.size();

	return mean;
}

std::vector<Point2D> circle_intersection(const Point2D& p1, const Point2D& p2, float r) {
	assert(p1 != p2);
	float distanceBetweenPoints = Distance2D(p1, p2);
	assert(r > distanceBetweenPoints / 2);

	// remaining distance from center towards the intersection, using pythagoras
	float remainingDistanceFromCenter = std::sqrt((r * r) - ((distanceBetweenPoints / 2) * (distanceBetweenPoints / 2)));

	// center of both points
	Point2D offsetToCenter((p2.x - p1.x) / 2, (p2.y - p1.y) / 2);
	Point2D center = p1 + offsetToCenter;

	// stretch offset vector in the ratio of remaining distance from center to intersection
	float vectorStretchFactor = remainingDistanceFromCenter / (distanceBetweenPoints / 2);
	Point2D offsetToCenterStretched(offsetToCenter.x * vectorStretchFactor, offsetToCenter.y * vectorStretchFactor);

	// rotate the vector by 90 degrees
	Point2D vectorRotated1(offsetToCenterStretched.y, -offsetToCenterStretched.x);
	Point2D vectorRotated2(-offsetToCenterStretched.y, offsetToCenterStretched.x);
	Point2D intersect1 = center + vectorRotated1;
	Point2D intersect2 = center + vectorRotated2;

	return { intersect1, intersect2 };
}

Point2D towards(const Point2D& p1, const Point2D& p2, float distance)
{
	if (p1 == p2) {
		return p1;
	}

	Point2D p;
	float d = Distance2D(p1, p2);
	p.x = p1.x + (p2.x - p1.x) / d * distance;
	p.y = p1.y + (p2.y - p1.y) / d * distance;

	return p;
}
// ------------------ Helper Functions ------------------

int BasicSc2Bot::height_at(const Point2DI& p) const {
	HeightMap h_map(Observation()->GetGameInfo());
	return static_cast<int>(h_map.TerrainHeight(p));
}

void BasicSc2Bot::find_groups(std::vector<Point2D>& points, int minimum_points_per_group, int max_distance_between_points)
{
	const int NOT_INTERESTED = -2;
	const int NOT_COLORED_YET = -1;
	int currentColor = NOT_COLORED_YET;
	std::vector<std::vector<int>> picture(Observation()->GetGameInfo().height, std::vector<int>(Observation()->GetGameInfo().width, NOT_INTERESTED));

	auto paint = [&picture, &currentColor](const Point2DI& pt) {
		picture[pt.y][pt.x] = currentColor;
		};

	std::vector<Point2DI> nearby;
	for (int dx = -max_distance_between_points; dx <= max_distance_between_points; ++dx) {
		for (int dy = -max_distance_between_points; dy <= max_distance_between_points; ++dy) {
			if (abs(dx) + abs(dy) <= max_distance_between_points) {
				nearby.emplace_back(Point2DI(dx, dy));
			}
		}
	}

	for (const auto& point : points) {
		paint(point);
	}

	std::vector<Point2D> remaining(points.begin(), points.end());
	std::deque<Point2D> queue;

	while (!remaining.empty()) {
		std::vector<Point2D> currentGroup;
		if (queue.empty()) {
			currentColor++;
			auto start = remaining.back();
			remaining.pop_back();
			paint(start);
			queue.emplace_back(start);
			currentGroup.emplace_back(start);
		}
		while (!queue.empty()) {
			Point2DI base = Point2DI(queue.front());
			queue.pop_front();
			for (const auto& offset : nearby) {
				int px = base.x + offset.x;
				int py = base.y + offset.y;
				if (px < 0 || py < 0 || px >= Observation()->GetGameInfo().width || py >= Observation()->GetGameInfo().height) {
					continue;
				}
				if (picture[py][px] != NOT_COLORED_YET) {
					continue;
				}
				Point2D point(px, py);
				auto it = std::find(remaining.begin(), remaining.end(), point);
				if (it != remaining.end()) {
					remaining.erase(it);
					paint(point);
					queue.emplace_back(point);
					currentGroup.emplace_back(point);
				}
			}
		}
		if (currentGroup.size() >= minimum_points_per_group) {
			std::sort(currentGroup.begin(), currentGroup.end(),
				[this](const Point2D& a, const Point2D& b) {
					return height_at(Point2DI(a)) > height_at(Point2DI(b));
				});
			ramps.emplace_back(currentGroup);
		}
	}
	return;
}

void BasicSc2Bot::find_ramps()
{
	const ObservationInterface* obs = Observation();
	std::vector<Point2D> rampVec;
	unsigned int width = obs->GetGameInfo().width;
	unsigned int height = obs->GetGameInfo().height;

	for (unsigned int i = 0; i < width; ++i)
	{
		for (unsigned int j = 0; j < height; ++j)
		{
			Point2D temp(i, j);
			if (obs->IsPathable(temp) && !obs->IsPlacable(temp))
			{
				rampVec.emplace_back(Point2D(i, j));
			}

		}
	}
	find_groups(rampVec, 8, 2);
	return;
}

std::vector<Point2D> BasicSc2Bot::upper_lower(const std::vector<Point2D>& points, bool up) const
{
	std::vector<Point2D> up_low_points;
	int height;

	if (up) {

		height = height_at(Point2DI(points[0]));
		up_low_points.emplace_back(points[0]);

		for (size_t i = 1; i < points.size(); ++i) {
			if (height_at(Point2DI(points[i])) == height) {
				up_low_points.emplace_back(points[i]);
			}
		}
	}
	else {
		height = height_at(Point2DI(points[points.size() - 1]));
		up_low_points.emplace_back(points[points.size() - 1]);

		for (size_t i = points.size() - 2; i > 0; --i) {
			if (height_at(Point2DI(points[i])) == height) {
				up_low_points.emplace_back(points[i]);
			}
		}
	}
	return up_low_points;
}

Point2D BasicSc2Bot::top_bottom_center(const std::vector<Point2D>& points, const bool up) const
{
	std::vector<Point2D> top_bottom_points;
	if(up) {
		top_bottom_points = upper_lower(points, up);
	}
	else {
		top_bottom_points = upper_lower(points, up);
	}
	
	return Point2D_mean(top_bottom_points);
}

std::vector<Point2D> BasicSc2Bot::upper2_for_ramp_wall(const std::vector<Point2D>& points) const
{
	std::vector<Point2D> upper = upper_lower(points, true);
	Point2D bottom_center = top_bottom_center(points, false);

	std::sort(upper.begin(), upper.end(),
		[this, &bottom_center](const Point2D& a, const Point2D& b) {
			return Distance2D(a, bottom_center) > Distance2D(b, bottom_center);
		});
	return { upper[0], upper[1] };
}

Point2D BasicSc2Bot::depot_barrack_in_middle(const std::vector<Point2D>& points, const std::vector<Point2D>& upper2, const bool isdepot) const
{
	Point2D offset(0.5, 0.5);
	const float inter_distance = isdepot ? 2.5 : 5;
	std::vector<Point2D> offsetUpper2 = { upper2[0] + offset, upper2[1] + offset };
	std::vector<Point2D> intersects = circle_intersection(offsetUpper2[0], offsetUpper2[1], std::sqrt(inter_distance));
	std::vector<Point2D> lower = upper_lower(points, false);

	Point2D anyLowerPoint = lower[0];
	return *std::max_element(intersects.begin(), intersects.end(),
		[&anyLowerPoint](const Point2D& a, const Point2D& b) {
			return Distance2D(a, anyLowerPoint) < Distance2D(b, anyLowerPoint);
		});
}

std::vector<Point2D> BasicSc2Bot::corner_depots(const std::vector<Point2D>& points) const
{
	std::vector<Point2D> corner_depots;
	std::vector<Point2D> upper2 = upper2_for_ramp_wall(points);
	Point2D offset(0.5, 0.5);

	for (const auto& p : upper2)
	{
		corner_depots.emplace_back(p + offset);
	}

	Point2D center = towards(corner_depots[0], corner_depots[1], (Distance2D(corner_depots[0], corner_depots[1])) / 2);
	Point2D depotPosition = depot_barrack_in_middle(points, upper2, true);
	std::vector<Point2D> intersects = circle_intersection(center, depotPosition, std::sqrt(5));
	std::sort(intersects.begin(), intersects.end(),
		[](const Point2D& a, const Point2D& b) {
			return a.x > b.x;
		});

	return intersects;
}

bool BasicSc2Bot::barracks_can_fit_addon(const Point2D& barrack_point) const
{
	return (barrack_point.x + 1) > (mainBase_depot_points[0].x);
}

Point2D BasicSc2Bot::barracks_correct_placement(const std::vector<Point2D>& ramp_points, const std::vector<Point2D>& corner_depots) const
{
	Point2D bpoint = depot_barrack_in_middle(ramp_points, upper2_for_ramp_wall(ramp_points), false);
	if (barracks_can_fit_addon(bpoint))
	{
		return bpoint;
	}
	else
	{
		return Point2D(bpoint.x - 2, bpoint.y);
	}
}

void BasicSc2Bot::find_right_ramp(const Point2D& location)
{
	find_ramps();
	//location could be start location or any other command center location
	// find the ramp set that is closest to the location

	std::sort(ramps.begin(), ramps.end(),
		[&location](const std::vector<Point2D>& a, const std::vector<Point2D>& b) {
			return Distance2D(Point2D_mean(a), location) < Distance2D(Point2D_mean(b), location);
		});

	std::vector<Point2D> main_ramp;
	ramps[0].size() < ramps[1].size() ? main_ramp = ramps[0] : main_ramp = ramps[1];

	//! right_ramp.size() == 2 and they are correct
	mainBase_depot_points = corner_depots(main_ramp);
	mainBase_barrack_point = barracks_correct_placement(main_ramp, mainBase_depot_points);
	return;
}

void BasicSc2Bot::depot_control() {
	const ObservationInterface* obs = Observation();
	Units depots = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));
	Units lowered_depots = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED));
	Units enemy_units = obs->GetUnits(Unit::Alliance::Enemy);

	// Raise depots when enemies are nearby
	for (const auto& depo : depots) {
		bool enemy_nearby = false;
		for (const auto& unit : enemy_units) {
			if (IsTrivialUnit(unit)) continue;
			if (Distance2D(unit->pos, depo->pos) < 15) {
				enemy_nearby = true;
				break;
			}
		}
		if (!enemy_nearby) {
			Actions()->UnitCommand(depo, ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
		}
	}

	// Lower depots when no enemies are nearby
	for (const auto& depo : lowered_depots) {
		for (const auto& unit : enemy_units) {
			if (IsTrivialUnit(unit)) continue;
			if (Distance2D(unit->pos, depo->pos) < 10) {
				Actions()->UnitCommand(depo, ABILITY_ID::MORPH_SUPPLYDEPOT_RAISE);
				break;
			}
		}
	}
}


