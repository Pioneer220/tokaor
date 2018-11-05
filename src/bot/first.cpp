#include "bot/first.hpp"
#include "bot/frame.hpp"
#include "bot/game_clone.hpp"

Plan::Plan()
  : path(Path()),
    execution_step(0)
{
}

Plan::Plan(Path path)
  : path(path),
    execution_step(0)
{
}

bool Plan::is_finished() {
    return execution_step >= path.size();
}

hlt::Direction Plan::next_move() const {
    return path[execution_step];
}

void Plan::advance() {
    execution_step++;
}

hlt::Game FirstBot::advance_game(hlt::Game& game, std::vector<hlt::Command> moves)
{
	return hlt::Game();
}

FirstBot::FirstBot(unsigned int seed)
    : rng(seed)
{
	current_bot_state = bot_state::building_and_gathering;
}

void FirstBot::init(hlt::Game& game) {
    // Last thing to call
    game.ready("FirstBot");
}

std::vector<hlt::Command> FirstBot::run(const hlt::Game& game) {
    Frame frame(game);
	//Make game clone
	GameClone game_clone(frame, true, std::max(game.game_map->width, game.game_map->height));

    auto player = frame.get_game().me;

    std::vector<hlt::Command> commands;

	//Updating clone map
	for (auto& pair : player->ships) {
		auto& ship = pair.second;
		game_clone.advance_game(plans[ship->id], *ship);
	}

	//If in building and gathering state
	if (current_bot_state == bot_state::building_and_gathering) 
	{
		//build ship
		if (player->halite >= 1000 && !game.game_map->at(player->shipyard)->is_occupied() && !game_clone.is_cell_occupied(player->shipyard->position, 1)) {
			commands.push_back(player->shipyard->spawn());
		}

		//stop building, start hoarding
		if ((float)(game.turn_number) / (float)(hlt::constants::MAX_TURNS) > 0.4f) {
			hlt::log::log("ENTERING STATE: hoarding_and_gathering");
			current_bot_state = bot_state::hoarding_and_gathering;
		}
	}
	if (current_bot_state == bot_state::building_and_gathering || current_bot_state == bot_state::hoarding_and_gathering) 
	{
		//Stop gathering, start returning
		if ((float)(game.turn_number) / (float)(hlt::constants::MAX_TURNS) > 0.95f) {
			hlt::log::log("ENTERING STATE: returning");
			current_bot_state = bot_state::returning;
		}
	}
	if (current_bot_state == bot_state::returning) 
	{
		for (auto& pair : player->ships) {
			auto id = pair.first;
			auto& ship = pair.second;

			//TODO get closest deploy position
			auto path = frame.get_direct_path(game_clone.get_map(), *ship, player->shipyard->position);
			plans[id] = Plan(path);
		}
	}

	//TODO make direct path to closeset deploy station when returning
    std::unordered_map<hlt::EntityId, hlt::Direction> moves;
	if (current_bot_state != bot_state::returning) {
		for (auto& pair : player->ships) {
			auto id = pair.first;
			auto& ship = pair.second;

			// Create plans if necessary.
			if (plans[ship->id].is_finished()) {
				//auto path = frame.get_optimal_path(*ship, player->shipyard->position);

				//Make path on the map clone
				auto path = frame.get_optimal_path(game_clone.get_map(), *ship, player->shipyard->position);

				plans[id] = Plan(path);

				//Update clone map with current plan
				game_clone.advance_game(plans[id], *ship);
			}
			moves[id] = plans[id].next_move();
		}

		//Collision avoidance, 
		//TODO -> ignore collision avoidance at shipyard when rturning
		auto new_moves = frame.avoid_collisions(moves);

		// Find moves for each ship, avoiding collisions.
		for (auto& pair : player->ships) {
			auto id = pair.first;
			auto& ship = pair.second;

			if (moves[id] == new_moves[id]) {
				plans[id].advance();
			}
			commands.push_back(ship->move(new_moves[id]));
		}
	}
	if(current_bot_state == bot_state::returning)
	{
		//Go straight for shipyard
		for (auto& pair : player->ships) 
		{
			auto& ship = pair.second;
			if (!plans[ship->id].is_finished())
			{
				commands.push_back(ship->move(plans[ship->id].next_move()));
			}
			else 
			{
				commands.push_back(ship->move(hlt::Direction::STILL));
			}
		}
	}

	//Print prediction
	//hlt::log::log(game_clone.print_prediction());

    return commands;
}
