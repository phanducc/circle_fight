#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <tmxlite/Map.hpp>
#include <tmxlite/Layer.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <tmxlite/Property.hpp> 
#include <tmxlite/Object.hpp>   

using namespace std;

const int W = 800;
const int H = 600;

const float PLAYER_SIZE = 20.f; 
const float ENEMY_R = 10.f;

enum GameState { MENU, PLAYING, WIN };

void centerText(sf::Text& text, float y) {
    sf::FloatRect textRect = text.getLocalBounds();
    text.setOrigin(textRect.left + textRect.width/2.0f, textRect.top  + textRect.height/2.0f);
    text.setPosition(sf::Vector2f(W/2.0f, y));
}

bool checkCollision(const sf::RectangleShape& rect, const sf::CircleShape& circle) {
    float cx = circle.getPosition().x;
    float cy = circle.getPosition().y;
    
    float rx = rect.getPosition().x - rect.getOrigin().x;
    float ry = rect.getPosition().y - rect.getOrigin().y;
    float rw = rect.getSize().x;
    float rh = rect.getSize().y;

    float testX = cx;
    float testY = cy;

    if (cx < rx) testX = rx;
    else if (cx > rx + rw) testX = rx + rw;
    
    if (cy < ry) testY = ry;
    else if (cy > ry + rh) testY = ry + rh;

    float distX = cx - testX;
    float distY = cy - testY;
    float distance = sqrt((distX * distX) + (distY * distY));

    return distance <= circle.getRadius();
}

class Enemy {
public:
    sf::CircleShape shape;
    sf::Vector2f startPos, endPos;
    float speed;
    bool movingToEnd;

    Enemy(float sx, float sy, float ex, float ey, float spd) {
        shape.setRadius(ENEMY_R);
        shape.setFillColor(sf::Color::Blue); 
        shape.setOutlineThickness(2.f);
        shape.setOutlineColor(sf::Color::Black);
        shape.setOrigin(ENEMY_R, ENEMY_R);
        shape.setPosition(sx, sy);
        
        startPos = sf::Vector2f(sx, sy);
        endPos = sf::Vector2f(ex, ey);
        speed = spd;
        movingToEnd = true;
    }

    void update() {
        sf::Vector2f target = movingToEnd ? endPos : startPos;
        sf::Vector2f current = shape.getPosition();
        
        float dx = target.x - current.x;
        float dy = target.y - current.y;
        float dist = sqrt(dx*dx + dy*dy);

        if (dist <= speed) {
            shape.setPosition(target);
            movingToEnd = !movingToEnd; 
        } else {
            shape.move((dx/dist) * speed, (dy/dist) * speed);
        }
    }
};

class Player {
public:
    sf::RectangleShape shape;
    float speed;

    Player() : speed(4.0f) {
        shape.setSize(sf::Vector2f(PLAYER_SIZE, PLAYER_SIZE));
        shape.setFillColor(sf::Color::Red);
        shape.setOutlineThickness(2.f);
        shape.setOutlineColor(sf::Color::Black);
        shape.setOrigin(PLAYER_SIZE/2.f, PLAYER_SIZE/2.f);
    }

    void setPosition(float x, float y) {
        shape.setPosition(x, y);
    }

    sf::Vector2f getPosition() const {
        return shape.getPosition();
    }

    void update(const vector<sf::RectangleShape>& walls) {
        float moveX = 0;
        float moveY = 0;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::A))  moveX = -speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)|| sf::Keyboard::isKeyPressed(sf::Keyboard::D)) moveX = speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)   || sf::Keyboard::isKeyPressed(sf::Keyboard::W))  moveY = -speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down) || sf::Keyboard::isKeyPressed(sf::Keyboard::S))  moveY = speed;

        shape.move(moveX, 0);
        for (const auto& wall : walls) {
            if (shape.getGlobalBounds().intersects(wall.getGlobalBounds())) {
                shape.move(-moveX, 0); break;
            }
        }

        shape.move(0, moveY);
        for (const auto& wall : walls) {
            if (shape.getGlobalBounds().intersects(wall.getGlobalBounds())) {
                shape.move(0, -moveY); break;
            }
        }
    }

    void draw(sf::RenderWindow& window) {
        window.draw(shape);
    }
};

class Level {
public:
    vector<Enemy> enemies;
    vector<sf::RectangleShape> walls;
    sf::RectangleShape startZone;
    sf::RectangleShape endZone;

    Level() {
        sf::Color pinkColor(251, 203, 223, 180); 
        startZone.setFillColor(pinkColor);
        endZone.setFillColor(pinkColor);
    }

    void loadFromFile(int levelNum, Player& player) {
        enemies.clear();
        walls.clear();
        
        tmx::Map map;
        string mapFilename = "level" + to_string(levelNum) + ".tmx";
        
        if (map.load(mapFilename)) {
            const auto& layers = map.getLayers();
            for (const auto& layer : layers) {
                if (layer->getType() == tmx::Layer::Type::Object) {
                    const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();
                    
                    for (const auto& object : objectLayer.getObjects()) {
                        auto rect = object.getAABB();
                        string name = object.getName();
                        
                        if (name == "Wall") {
                            sf::RectangleShape wall(sf::Vector2f(rect.width, rect.height));
                            wall.setPosition(rect.left, rect.top);
                            wall.setFillColor(sf::Color(40, 40, 40)); 
                            walls.push_back(wall);
                        } 
                        else if (name == "Start") {
                            startZone.setPosition(rect.left, rect.top);
                            startZone.setSize(sf::Vector2f(rect.width, rect.height));
                            player.setPosition(rect.left + rect.width/2.f, rect.top + rect.height/2.f);
                        } 
                        else if (name == "End") {
                            endZone.setPosition(rect.left, rect.top);
                            endZone.setSize(sf::Vector2f(rect.width, rect.height));
                        }
                        else if (name == "Enemy") {
                            float speed = 4.0f; 
                            for (const auto& prop : object.getProperties()) {
                                if (prop.getName() == "speed" && prop.getType() == tmx::Property::Type::Float) {
                                    speed = prop.getFloatValue();
                                }
                            }
                            if (object.getShape() == tmx::Object::Shape::Polyline && object.getPoints().size() >= 2) {
                                float objX = object.getPosition().x;
                                float objY = object.getPosition().y;
                                float startX = objX + object.getPoints()[0].x;
                                float startY = objY + object.getPoints()[0].y;
                                float endX = objX + object.getPoints()[1].x;
                                float endY = objY + object.getPoints()[1].y;
                                enemies.push_back(Enemy(startX, startY, endX, endY, speed));
                            }
                        }
                    }
                }
            }
        } 
    }

    void updateEnemies() {
        for (auto& e : enemies) e.update();
    }

    bool isPlayerHit(const Player& player) {
        for (auto& e : enemies) {
            if (checkCollision(player.shape, e.shape)) return true;
        }
        return false;
    }

    bool isPlayerInEndZone(const Player& player) {
        return endZone.getGlobalBounds().contains(player.getPosition());
    }

    void draw(sf::RenderWindow& window) {
        for (const auto& w : walls) window.draw(w); 
        window.draw(startZone); 
        window.draw(endZone);
        for (const auto& e : enemies) window.draw(e.shape);
    }
};

class Game {
private:
    sf::RenderWindow window;
    sf::Font font;
    GameState state;
    
    Player player;
    Level level;

    int currentLevel;
    int maxLevel;
    sf::Clock gameClock;
    float bestTime;

    sf::Text levelText, timeText, highscoreText;
    sf::Text menuTitle, menuPrompt;
    sf::Text winTitle, winScore, winPrompt;

public:
    Game() : window(sf::VideoMode(W, H), "Circle Fight"),
             state(MENU), currentLevel(1), maxLevel(3) {
        window.setFramerateLimit(60);
        
        if (!font.loadFromFile("BitcountSingle_Roman-Regular.ttf")) {
            font.loadFromFile("C:/Windows/Fonts/arial.ttf"); 
        }
        
        bestTime = loadHighScore();
        initUI();
    }

    void run() {
        while (window.isOpen()) {
            processEvents();
            update();
            render();
        }
    }

private:
    float loadHighScore() {
        ifstream file("highscore.txt");
        float bTime = 999999.f; 
        if (file.is_open()) {
            string chuDiem, chuCao;
            if (file >> chuDiem >> chuCao >> bTime) {}
            file.close();
        }
        return bTime;
    }

    void saveHighScore(float newTime) {
        ofstream file("highscore.txt");
        if (file.is_open()) {
            file << "Diem cao: " << fixed << setprecision(2) << newTime;
            file.close();
        }
    }

    void initUI() {
        levelText = sf::Text("LEVEL: 1 / 3", font, 24);
        levelText.setFillColor(sf::Color::Black);
        levelText.setPosition(10, 10);

        timeText = sf::Text("TIME: 0.00s", font, 24);
        timeText.setFillColor(sf::Color::Red);
        timeText.setPosition(W - 180, 10);

        highscoreText = sf::Text("BEST: N/A", font, 20);
        highscoreText.setFillColor(sf::Color(0, 100, 0)); 
        highscoreText.setPosition(W - 180, 40);

        menuTitle = sf::Text("Circle Fight", font, 80);
        menuTitle.setFillColor(sf::Color::Black);
        centerText(menuTitle, 200);

        menuPrompt = sf::Text("Press Any Key to Start", font, 30);
        menuPrompt.setFillColor(sf::Color::Red);
        centerText(menuPrompt, 400);

        winTitle = sf::Text("Winner Winner Chicken Dinner", font, 50);
        winTitle.setFillColor(sf::Color(200, 50, 50)); 
        centerText(winTitle, 150);

        winScore = sf::Text("", font, 35);
        winScore.setFillColor(sf::Color(0, 100, 0));

        winPrompt = sf::Text("Press Space or Enter to Play Again", font, 25);
        winPrompt.setFillColor(sf::Color::Black);
        centerText(winPrompt, 450);
    }

    void processEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            
            if (event.type == sf::Event::KeyPressed) {
                if (state == MENU || state == WIN) {
                    if (event.key.code == sf::Keyboard::Space || event.key.code == sf::Keyboard::Enter) {
                        state = PLAYING;
                        currentLevel = 1;
                        level.loadFromFile(currentLevel, player);
                        gameClock.restart();
                    }
                }
            }
        }
    }

    void update() {
        if (state == PLAYING) {
            player.update(level.walls);
            level.updateEnemies();

            if (level.isPlayerHit(player)) {
                level.loadFromFile(currentLevel, player); 
            }

            if (level.isPlayerInEndZone(player)) {
                currentLevel++;
                
                if (currentLevel > maxLevel) {
                    float finalTime = gameClock.getElapsedTime().asSeconds();
                    bool isNewRecord = false;
                    
                    if (finalTime < bestTime) {
                        bestTime = finalTime;
                        saveHighScore(bestTime); 
                        isNewRecord = true;
                    }
                    
                    stringstream ss;
                    if (isNewRecord) {
                        ss << "NEW RECORD: " << fixed << setprecision(2) << finalTime << "s !\n\n";
                    } else {
                        ss << "Your Time: " << fixed << setprecision(2) << finalTime << "s\n\n";
                        ss << "Highscore: " << fixed << setprecision(2) << bestTime << "s";
                    }
                    winScore.setString(ss.str());
                    centerText(winScore, 300);

                    state = WIN;
                } else {
                    level.loadFromFile(currentLevel, player);
                }
            }

            levelText.setString("LEVEL: " + to_string(currentLevel) + " / " + to_string(maxLevel));
            
            float currentTime = gameClock.getElapsedTime().asSeconds();
            stringstream ssTime;
            ssTime << "TIME: " << fixed << setprecision(2) << currentTime << "s"; 
            timeText.setString(ssTime.str());

            stringstream ssBest;
            if (bestTime < 999999.f) {
                ssBest << "BEST: " << fixed << setprecision(2) << bestTime << "s";
            } else {
                ssBest << "BEST: N/A";
            }
            highscoreText.setString(ssBest.str());
        }
    }

    void render() {
        window.clear(sf::Color(240, 248, 255)); 
        
        if (state == MENU) {
            window.draw(menuTitle);
            window.draw(menuPrompt);
        } 
        else if (state == PLAYING) {
            level.draw(window);
            player.draw(window);
            window.draw(levelText);
            window.draw(timeText);
            window.draw(highscoreText);
        } 
        else if (state == WIN) {
            window.draw(winTitle);
            window.draw(winScore);
            window.draw(winPrompt);
        }

        window.display();
    }
};

int main() {
    Game myGame;
    myGame.run();
    return 0;
}