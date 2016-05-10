/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * panaconda.cpp -- example of usage c++ bindings in nvml
 */
#include <cstdlib>
#include <ctime>
#include <getopt.h>
#include <iostream>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#include "panaconda.hpp"

#define LAYOUT_NAME "pAnaconda"
#define DEFAULT_DELAY 120000

#define SNAKE_START_POS_X 5
#define SNAKE_START_POS_Y 5
#define SNAKE_START_DIR (direction::RIGHT)
#define SNAKE_STAR_SEG_NO 5

#define BOARD_STATIC_SIZE_ROW 40
#define BOARD_STATIC_SIZE_COL 30

#define PLAYER_POINTS_PER_HIT 10

using nvml::transaction_error;
using nvml::obj::transaction;
using nvml::obj::persistent_ptr;
using nvml::obj::pool;
using nvml::obj::make_persistent;
using nvml::obj::delete_persistent;
using examples::list;


/*
 * ColorPair
 */
ColorPair::ColorPair() : colorBg(COLOR_BLACK), colorFg(COLOR_BLACK)
{
}
ColorPair::ColorPair(const int aColFg, const int aColBg)
	: colorBg(aColBg), colorFg(aColFg)
{
}

/*
 * Helper
 */
struct ColorPair
Helper::getColor(const object_type aShape)
{
	struct ColorPair res;
	switch (aShape) {
		case SNAKE_SEGMENT:
			res = ColorPair(COLOR_WHITE, COLOR_BLACK);
			break;
		case WALL:
			res = ColorPair(COLOR_BLUE, COLOR_BLUE);
			break;
		case FOOD:
			res = ColorPair(COLOR_RED, COLOR_BLACK);
			break;
		default:
			std::cout << "Error: getColor - wrong value passed!"
				<< std::endl;
			assert(0);
	}
	return res;
}

int
Helper::parseParams(int argc, char *argv[], struct Params *params)
{
	int opt;
	std::string app = argv[0];
	while ((opt = getopt(argc, argv, "m:")) != -1) {
		switch (opt) {
			case 'm':
				params->use_maze = true;
				params->maze_path = optarg;
				break;
			default:
				Helper::print_usage(app);
				return -1;
		}
	}

	if (optind < argc) {
		params->name = argv[optind];
	} else {
		Helper::print_usage(app);
		return -1;
	}
	return 0;
}

inline void
Helper::sleep(int aTime)
{
	clock_t currTime = clock();
	while (clock() < (currTime + aTime)) {
	}
}

inline void
Helper::print_usage(std::string &name)
{
	std::cout << "Usage: " << name
		  << " [-m <maze_path>] <pool_name>\n";
}

/*
 * Point
 */
Point::Point() : mX(0), mY(0)
{
}

Point::Point(int aX, int aY) : mX(aX), mY(aY)
{
}

bool
operator==(Point &aPoint1, Point &aPoint2)
{
	return aPoint1.mX == aPoint2.mX && aPoint1.mY == aPoint2.mY;
}
/*
 * Shape
 */
Shape::Shape(int aShape)
{
	int nCurvesSymbol = getSymbol(aShape);
	mVal = COLOR_PAIR(aShape) | nCurvesSymbol;
}

int
Shape::getVal()
{
	return mVal;
}

int
Shape::getSymbol(int aShape)
{
	int symbol = 0;
	switch (aShape) {
		case SNAKE_SEGMENT:
			symbol = ACS_DIAMOND;
			break;
		case WALL:
			symbol = ACS_BLOCK;
			break;
		case FOOD:
			symbol = ACS_CKBOARD;
			break;

		default:
			symbol = ACS_DIAMOND;
			break;
	}
	return symbol;
}

/*
 * Element
 */
Element::Element()
	: mPoint(make_persistent<Point>(0, 0)),
	  mShape(make_persistent<Shape>(SNAKE_SEGMENT)),
	  mDirection(direction::LEFT)
{
}

Element::Element(int aX, int aY, nvml::obj::persistent_ptr<Shape> aShape,
	direction aDir)
	: mPoint(make_persistent<Point>(aX, aY)),
	  mShape(aShape),
	  mDirection(aDir)
{
}

Element::Element(Point aPoint, nvml::obj::persistent_ptr<Shape> aShape,
	direction aDir)
	: mPoint(make_persistent<Point>(aPoint.mX, aPoint.mY)),
	  mShape(aShape),
	  mDirection(aDir)
{
}

Element::Element(const Element &aElement)
{
	mPoint = make_persistent<Point>(aElement.mPoint->mX,
						   aElement.mPoint->mY);
	mShape = make_persistent<Shape>(
		aElement.mShape->getVal());
}

Element::~Element()
{
	nvml::obj::delete_persistent<Point>(mPoint);
	mPoint = nullptr;
	nvml::obj::delete_persistent<Shape>(mShape);
	mShape = nullptr;
}

persistent_ptr<Point>
Element::calcNewPosition(const direction aDir)
{
	persistent_ptr<Point> point =
		make_persistent<Point>(mPoint->mX, mPoint->mY);

	switch (aDir) {
		case direction::DOWN:
			point->mY = point->mY + 1;
			break;
		case direction::LEFT:
			point->mX = point->mX - 1;
			break;
		case direction::RIGHT:
			point->mX = point->mX + 1;
			break;
		case direction::UP:
			point->mY = point->mY - 1;
			break;
		default:
			break;
	}

	return point;
}

void
Element::setPosition(const persistent_ptr<Point> aNewPoint)
{
	mPoint = aNewPoint;
}

persistent_ptr<Point>
Element::getPosition(void)
{
	return mPoint;
}

void
Element::print(void)
{
	mvaddch(mPoint->mY, mPoint->mX, mShape->getVal());
}

void
Element::printDoubleCol(void)
{
	mvaddch(mPoint->mY, (2 * mPoint->mX), mShape->getVal());
}

void
Element::printSingleDoubleCol(void)
{
	mvaddch(mPoint->mY, (2 * mPoint->mX), mShape->getVal());
	mvaddch(mPoint->mY, (2 * mPoint->mX - 1), mShape->getVal());
}

direction
Element::getDirection(void)
{
	return mDirection;
}
void
Element::setDirection(const direction aDir)
{
	mDirection = aDir;
}

/*
 * Snake
 */
Snake::Snake()
{
	mSnakeSegments = make_persistent<list<Element>>();
	for (unsigned i = 0; i < SNAKE_STAR_SEG_NO; ++i) {
		persistent_ptr<Shape> shape =
			make_persistent<Shape>(SNAKE_SEGMENT);
		persistent_ptr<Element> element = make_persistent<Element>(
			SNAKE_START_POS_X - i, SNAKE_START_POS_Y, shape,
			SNAKE_START_DIR);
		mSnakeSegments->push_back(element);
	}

	mLastSegPosition = Point();
	mLastSegDir = direction::RIGHT;
}

Snake::~Snake()
{
	mSnakeSegments->clear();
	delete_persistent<list<Element>>(mSnakeSegments);
}

void
Snake::move(const direction aDir)
{
	int snakeSize = mSnakeSegments->size();
	persistent_ptr<Point> newPositionPoint;

	mLastSegPosition =
		*(mSnakeSegments->get(snakeSize - 1)->getPosition().get());
	mLastSegDir = mSnakeSegments->get(snakeSize - 1)->getDirection();

	for (int i = (snakeSize - 1); i >= 0; --i) {
		if (i == 0) {
			newPositionPoint =
				mSnakeSegments->get(i)->calcNewPosition(aDir);
			mSnakeSegments->get(i)->setDirection(aDir);
		} else {
			newPositionPoint =
				mSnakeSegments->get(i)->calcNewPosition(
					mSnakeSegments->get(i - 1)
						->getDirection());
			mSnakeSegments->get(i)->setDirection(
				mSnakeSegments->get(i - 1)->getDirection());
		}
		mSnakeSegments->get(i)->setPosition(newPositionPoint);
	}
}

void
Snake::print(void)
{
	int i = 0;
	persistent_ptr<Element> segp;

	while ((segp = mSnakeSegments->get(i++)) != nullptr)
		segp->printDoubleCol();
}

void
Snake::addSegment(void)
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(SNAKE_SEGMENT);
	persistent_ptr<Element> segp =
		make_persistent<Element>(mLastSegPosition, shape, mLastSegDir);
	mSnakeSegments->push_back(segp);
}

bool
Snake::checkPointAgainstSegments(Point aPoint)
{
	int i = 0;
	bool result = false;
	persistent_ptr<Element> segp;

	while ((segp = mSnakeSegments->get(i++)) != nullptr) {
		if (aPoint == *(segp->getPosition().get())) {
			result = true;
			break;
		}
	}
	return result;
}

Point
Snake::getHeadPoint(void)
{
	return *(mSnakeSegments->get(0)->getPosition().get());
}

direction
Snake::getDirection(void)
{
	return mSnakeSegments->get(0)->getDirection();
}

Point
Snake::getNextPoint(const direction aDir)
{
	return *(mSnakeSegments->get(0)->calcNewPosition(aDir).get());
}

/*
 * Board
 */

Board::Board()
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(FOOD);
	mFood = make_persistent<Element>(0, 0, shape, direction::UNDEFINED);
	mLayout = make_persistent<list<Element>>();
	mSnake = make_persistent<Snake>();
	mSizeRow = 20;
	mSizeCol = 20;
}

Board::~Board()
{
	mLayout->clear();
	delete_persistent<list<Element>>(mLayout);
	delete_persistent<Snake>(mSnake);
	delete_persistent<Element>(mFood);
}

void
Board::print(const int aScore)
{
	const int offsetY = 2 * mSizeCol + 5;
	const int offsetX = 2;

	int i = 0;
	persistent_ptr<Element> elmp;

	while ((elmp = mLayout->get(i++)) != nullptr)
		elmp->printSingleDoubleCol();

	mSnake->print();
	mFood->printDoubleCol();

	mvprintw((offsetX + 0), offsetY, " ##### pAnaconda ##### ");
	mvprintw((offsetX + 1), offsetY, " #                   # ");
	mvprintw((offsetX + 2), offsetY, " #    q - quit       # ");
	mvprintw((offsetX + 3), offsetY, " #    n - new game   # ");
	mvprintw((offsetX + 4), offsetY, " #                   # ");
	mvprintw((offsetX + 5), offsetY, " ##################### ");

	mvprintw((offsetX + 7), offsetY, " Score: %d ", aScore);
}

void
Board::printGameOver(const int aScore)
{
	int x = mSizeCol / 3;
	int y = mSizeRow / 6;
	mvprintw(y + 0, x, "#######   #######   #     #   #######");
	mvprintw(y + 1, x, "#         #     #   ##   ##   #      ");
	mvprintw(y + 2, x, "#   ###   #######   # # # #   ####   ");
	mvprintw(y + 3, x, "#     #   #     #   #  #  #   #      ");
	mvprintw(y + 4, x, "#######   #     #   #     #   #######");

	mvprintw(y + 6, x, "#######   #     #    #######   #######");
	mvprintw(y + 7, x, "#     #   #     #    #         #     #");
	mvprintw(y + 8, x, "#     #    #   #     ####      #######");
	mvprintw(y + 9, x, "#     #     # #      #         #   #  ");
	mvprintw(y + 10, x, "#######      #       #######   #     #");

	mvprintw(y + 12, x, " Last score: %d ", aScore);
	mvprintw(y + 14, x, " q - quit");
	mvprintw(y + 15, x, " n - new game");
}

int
Board::creatDynamicLayout(const unsigned aRowNo, char *const aBuffer)
{
	persistent_ptr<Element> element;
	persistent_ptr<Shape> shape;

	for (unsigned i = 0; i < mSizeCol; ++i) {
		if (aBuffer[i] == config_file_symbol::SYM_WALL) {
			shape = make_persistent<Shape>(WALL);
			element = element = make_persistent<Element>(
				i, aRowNo, shape, direction::UNDEFINED);
			mLayout->push_back(element);
		}
	}
	return 0;
}

int
Board::creatStaticLayout(void)
{
	persistent_ptr<Element> element;
	persistent_ptr<Shape> shape;

	mSizeRow = BOARD_STATIC_SIZE_ROW;
	mSizeCol = BOARD_STATIC_SIZE_COL;

	// first and last row
	for (unsigned i = 0; i < mSizeCol; ++i) {
		shape = make_persistent<Shape>(WALL);
		element = make_persistent<Element>(i, 0, shape,
						   direction::UNDEFINED);
		mLayout->push_back(element);
		shape = make_persistent<Shape>(WALL);
		element = make_persistent<Element>(i, (mSizeRow - 1), shape,
						   direction::UNDEFINED);
		mLayout->push_back(element);
	}

	// middle rows
	for (unsigned i = 1; i < mSizeRow; ++i) {
		shape = make_persistent<Shape>(WALL);
		element = make_persistent<Element>(0, i, shape,
						   direction::UNDEFINED);
		mLayout->push_back(element);
		shape = make_persistent<Shape>(WALL);
		element = make_persistent<Element>((mSizeCol - 1), i, shape,
						   direction::UNDEFINED);
		mLayout->push_back(element);
	}
	return 0;
}

bool
Board::isSnakeCollision(Point aPoint)
{
	return mSnake->checkPointAgainstSegments(aPoint);
}

bool
Board::isWallCollision(Point aPoint)
{
	int i = 0;
	bool result = false;
	persistent_ptr<Element> wallp;

	while ((wallp = mLayout->get(i++)) != nullptr) {
		if (aPoint == *(wallp->getPosition().get())) {
			result = true;
			break;
		}
	}
	return result;
}

bool
Board::isCollision(Point aPoint)
{
	return isSnakeCollision(aPoint) || isWallCollision(aPoint);
}

bool
Board::isSnakeHeadFoodHit(void)
{
	bool result = false;
	Point headPoint = mSnake->getHeadPoint();

	if (headPoint == *(mFood->getPosition().get())) {
		result = true;
	}
	return result;
}

void
Board::setNewFood(const Point aPoint)
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(FOOD);
	delete_persistent<Element>(mFood);
	mFood = make_persistent<Element>(aPoint, shape, direction::UNDEFINED);
}

void
Board::createNewFood(void)
{
	const int maxRepeat = 50;
	int count = 0;
	int randRow = 0;
	int randCol = 0;

	while (count < maxRepeat) {
		randRow = 1 + rand() % (getSizeRow() - 2);
		randCol = 1 + rand() % (getSizeCol() - 2);

		Point foodPoint(randCol, randRow);
		if (!isCollision(foodPoint)) {
			setNewFood(foodPoint);
			break;
		}
		count++;
	}
}

snake_event
Board::moveSnake(const direction aDir)
{
	snake_event event = snake_event::EV_OK;
	Point nextPt = mSnake->getNextPoint(aDir);

	if (isCollision(nextPt)) {
		event = snake_event::EV_COLLISION;
	} else {
		mSnake->move(aDir);
	}

	return event;
}

void
Board::addSnakeSegment(void)
{
	mSnake->addSegment();
}

unsigned
Board::getSizeRow(void)
{
	return mSizeRow;
}
void
Board::setSizeRow(const unsigned aSizeRow)
{
	mSizeRow = aSizeRow;
}
unsigned
Board::getSizeCol(void)
{
	return mSizeCol;
}
void
Board::setSizeCol(const unsigned aSizeCol)
{
	mSizeCol = aSizeCol;
}

direction
Board::getSnakeDir(void)
{
	return mSnake->getDirection();
}

/*
 * GameState
 */
GameState::GameState()
{
}

GameState::~GameState()
{
}

nvml::obj::persistent_ptr<Board>
GameState::getBoard()
{
	return mBoard;
}

nvml::obj::persistent_ptr<Player>
GameState::getPlayer()
{
	return mPlayer;
}

void
GameState::init(void)
{
	mBoard = make_persistent<Board>();
	mPlayer = make_persistent<Player>();
}

void
GameState::cleanPool(void)
{
	delete_persistent<Board>(mBoard);
	mBoard = nullptr;

	delete_persistent<Player>(mPlayer);
	mPlayer = nullptr;
}

/*
 * Player
 */
Player::Player() : mScore(0), mState(STATE_PLAY)
{
}

Player::~Player()
{
}

int
Player::getScore(void)
{
	return mScore;
}

state
Player::getState(void)
{
	return mState;
}
void
Player::setState(const state aState)
{
	mState = aState;
}

void
Player::updateScore(void)
{
	mScore = mScore + PLAYER_POINTS_PER_HIT;
}
/*
 * Game
 */
Game::Game(struct Params *params)
{
	pool<GameState> pop;

	initscr();
	start_color();
	nodelay(stdscr, true);
	curs_set(0);
	keypad(stdscr, true);

	mParams = params;
	if (pool<GameState>::check(mParams->name, LAYOUT_NAME) == 1)
		pop = pool<GameState>::open(mParams->name, LAYOUT_NAME);
	else
		pop = pool<GameState>::create(mParams->name, LAYOUT_NAME,
					      PMEMOBJ_MIN_POOL * 10, 0666);

	mGameState = pop;
	mDirectionKey = direction::UNDEFINED;
	mLastKey = KEY_CLEAR;
	mDelay = DEFAULT_DELAY;

	initColors();

	srand(time(0));
}

Game::~Game()
{
}

void
Game::initColors(void)
{
	struct ColorPair colorPair = Helper::getColor(SNAKE_SEGMENT);
	init_pair(SNAKE_SEGMENT, colorPair.colorFg, colorPair.colorBg);

	colorPair = Helper::getColor(WALL);
	init_pair(WALL, colorPair.colorFg, colorPair.colorBg);

	colorPair = Helper::getColor(FOOD);
	init_pair(FOOD, colorPair.colorFg, colorPair.colorBg);
}

int
Game::init(void)
{
	int ret = 0;
	persistent_ptr<GameState> r = mGameState.get_root();

	if (r->getBoard() == nullptr) {
		try {
			transaction::manual tx(mGameState);
			r->init();
			if (mParams->use_maze)
				ret = parseConfCreateDynamicLayout();
			else
				ret = r->getBoard()->creatStaticLayout();

			r->getBoard()->createNewFood();
			transaction::commit();
		} catch (transaction_error &err) {
			std::cout << err.what() << std::endl;
		}
		if (ret) {
			cleanPool();
			clearProg();
			std::cout << "Error: Config file error!" << std::endl;
			return ret;
		}
	}
	mDirectionKey = r->getBoard()->getSnakeDir();
	return ret;
}

void
Game::processStep(void)
{
	snake_event retEvent = EV_OK;
	persistent_ptr<GameState> r = mGameState.get_root();
	try {
		transaction::exec_tx(mGameState, [&]() {
			retEvent = r->getBoard()->moveSnake(mDirectionKey);
			if (EV_COLLISION == retEvent) {
				r->getPlayer()->setState(state::STATE_GAMEOVER);
				return;
			} else {
				if (r->getBoard()->isSnakeHeadFoodHit()) {
					r->getBoard()->createNewFood();
					r->getBoard()->addSnakeSegment();
					r->getPlayer()->updateScore();
				}
			}
		});
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
	}

	r->getBoard()->print(r->getPlayer()->getScore());
}

inline bool
Game::isStopped(void)
{
	return action::ACTION_QUIT == mLastKey;
}

void
Game::setDirectionKey(void)
{
	switch (mLastKey) {
		case KEY_LEFT:
			if (direction::RIGHT != mDirectionKey)
				mDirectionKey = direction::LEFT;
			break;
		case KEY_RIGHT:
			if (direction::LEFT != mDirectionKey)
				mDirectionKey = direction::RIGHT;
			break;
		case KEY_UP:
			if (direction::DOWN != mDirectionKey)
				mDirectionKey = direction::UP;
			break;
		case KEY_DOWN:
			if (direction::UP != mDirectionKey)
				mDirectionKey = direction::DOWN;
			break;
		default:
			break;
	}
}

int
Game::processKey(const int aLastKey)
{
	int ret = 0;
	mLastKey = aLastKey;
	setDirectionKey();

	if (action::ACTION_NEW_GAME == aLastKey) {
		cleanPool();
		ret = init();
	}
	return ret;
}

void
Game::cleanPool(void)
{
	persistent_ptr<GameState> r = mGameState.get_root();

	try {
		transaction::exec_tx(mGameState, [&]() { r->cleanPool(); });
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
	}
}

void
Game::delay(void)
{
	Helper::sleep(mDelay);
}

void
Game::clearScreen(void)
{
	erase();
}

void
Game::gameOver(void)
{
	persistent_ptr<GameState> r = mGameState.get_root();
	r->getBoard()->printGameOver(r->getPlayer()->getScore());
}

bool
Game::isGameOver(void)
{
	persistent_ptr<GameState> r = mGameState.get_root();
	return (r->getPlayer()->getState() == state::STATE_GAMEOVER);
}

void
Game::clearProg(void)
{
	mGameState.close();
	endwin();
}

int
Game::parseConfCreateDynamicLayout(void)
{
	FILE *cfgFile;
	char *line = NULL;
	size_t len = 0;
	unsigned i = 0;
	ssize_t colNo = 0;

	cfgFile = fopen(mParams->maze_path.c_str(), "r");
	if (cfgFile == NULL)
		return -1;

	persistent_ptr<GameState> r = mGameState.get_root();
	while ((colNo = getline(&line, &len, cfgFile)) != -1) {
		if (i == 0)
			r->getBoard()->setSizeCol(colNo - 1);

		try {
			transaction::exec_tx(mGameState, [&]() {
				r->getBoard()->creatDynamicLayout(i, line);
			});
		} catch (transaction_error &err) {
			std::cout << err.what() << std::endl;
		}
		i++;
	}
	r->getBoard()->setSizeRow(i);

	free(line);
	fclose(cfgFile);
	return 0;
}

/*
 * main
 */
int
main(int argc, char *argv[])
{
	int input;
	Game *game;
	struct Params params;
	params.use_maze = false;

	if (Helper::parseParams(argc, argv, &params))
		return -1;

	game = new Game(&params);
	if (game->init())
		return -1;

	while (!game->isStopped()) {
		input = getch();
		if (game->processKey(input))
			return -1;
		if (game->isGameOver()) {
			game->gameOver();
		} else {
			game->delay();
			game->clearScreen();
			game->processStep();
		}
	}

	game->clearProg();
	return 0;
}
