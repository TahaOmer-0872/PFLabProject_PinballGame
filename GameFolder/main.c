#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float x;
    float y;
    float radius;
    Texture2D texture;
} Planet;

typedef struct {
    float x;
    float y;
} Vec2f;

typedef struct {
    Vec2f pivotPoint;
    float length;
    float width;
    float currentAngle;      
    float restingAngle;  
    float activeAngle;  
    float rotationSpeedDeg;  
    bool isLeftFlipper;
    Color color;
} Flipper;

typedef struct {
    float x;
    float y;
    float radius;
    float velocityX;
    float velocityY;
} Ball;

static Vec2f RotatePoint(Vec2f point, Vec2f pivot, float angle) {
    float sinAngle = sinf(angle), cosAngle = cosf(angle);
    float translatedX = point.x - pivot.x;
    float translatedY = point.y - pivot.y;
    Vec2f rotatedPoint = { 
        translatedX * cosAngle - translatedY * sinAngle + pivot.x, 
        translatedX * sinAngle + translatedY * cosAngle + pivot.y 
    };
    return rotatedPoint;
}

static float ClosestPointOnSegment(Vec2f segmentStart, Vec2f segmentEnd, Vec2f point, Vec2f *closestPoint) {
    float segmentDirX = segmentEnd.x - segmentStart.x;
    float segmentDirY = segmentEnd.y - segmentStart.y;
    float pointDirX = point.x - segmentStart.x;
    float pointDirY = point.y - segmentStart.y;
    float segmentLengthSquared = segmentDirX * segmentDirX + segmentDirY * segmentDirY;
    float projectionParameter = 0.0f;
    
    if (segmentLengthSquared > 1e-8f) {
        projectionParameter = (pointDirX * segmentDirX + pointDirY * segmentDirY) / segmentLengthSquared;
    }
    
    if (projectionParameter < 0.0f) projectionParameter = 0.0f;
    if (projectionParameter > 1.0f) projectionParameter = 1.0f;
    
    if (closestPoint) { 
        closestPoint->x = segmentStart.x + segmentDirX * projectionParameter; 
        closestPoint->y = segmentStart.y + segmentDirY * projectionParameter; 
    }
    return projectionParameter;
}

static bool CircleSegmentCollision(Vec2f segmentStart, Vec2f segmentEnd, Ball *ball) {
    Vec2f closestPoint;
    ClosestPointOnSegment(segmentStart, segmentEnd, (Vec2f){ball->x, ball->y}, &closestPoint);
    float deltaX = ball->x - closestPoint.x;
    float deltaY = ball->y - closestPoint.y;
    float distanceSquared = deltaX * deltaX + deltaY * deltaY;
    return (distanceSquared <= (ball->radius * ball->radius));
}

static void ReflectVelocity(Ball *ball, float normalX, float normalY, float bounceFactor) {
    float dotProduct = ball->velocityX * normalX + ball->velocityY * normalY;
    ball->velocityX -= 2.0f * dotProduct * normalX;
    ball->velocityY -= 2.0f * dotProduct * normalY;
    ball->velocityX *= bounceFactor;
    ball->velocityY *= bounceFactor;
}

static void SeparateCircleFromSegment(Ball *ball, Vec2f segmentStart, Vec2f segmentEnd) {
    Vec2f closestPoint;
    ClosestPointOnSegment(segmentStart, segmentEnd, (Vec2f){ball->x, ball->y}, &closestPoint);
    float deltaX = ball->x - closestPoint.x;
    float deltaY = ball->y - closestPoint.y;
    float distance = sqrtf(deltaX * deltaX + deltaY * deltaY);
    
    if (distance < 1e-5f) distance = 1e-5f;
    
    if (distance < ball->radius) {
        float normalX = deltaX / distance;
        float normalY = deltaY / distance;
        ball->x = closestPoint.x + normalX * ball->radius;
        ball->y = closestPoint.y + normalY * ball->radius;
    }
}

static void DrawFlipper(const Flipper *flipper) {
    Vec2f startPoint = flipper->pivotPoint;
    Vec2f endPoint = { 
        flipper->pivotPoint.x + flipper->length * cosf(flipper->currentAngle), 
        flipper->pivotPoint.y + flipper->length * sinf(flipper->currentAngle) 
    };
    DrawLineEx((Vector2){startPoint.x, startPoint.y}, (Vector2){endPoint.x, endPoint.y}, flipper->width, flipper->color);
    DrawCircleV((Vector2){startPoint.x, startPoint.y}, flipper->width * 0.5f, flipper->color);
    DrawCircleV((Vector2){endPoint.x, endPoint.y}, flipper->width * 0.5f, flipper->color);
}

int main(void) {
    const int SCREEN_WIDTH = 600, SCREEN_HEIGHT = 900;
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SPACE PINBALL");
    SetTargetFPS(60);
    InitAudioDevice();

    Texture2D background = LoadTexture("spacebg.jpg");
    Sound collisionSound = LoadSound("hit.wav");

    const int PLANET_COUNT = 6;
    Planet planets[PLANET_COUNT];
    planets[0] = (Planet){270, 320, 85, LoadTexture("earth.png")};
    planets[1] = (Planet){480, 120, 55, LoadTexture("mars.png")};
    planets[2] = (Planet){90, 120, 75, LoadTexture("jup.png")};
    planets[3] = (Planet){480, 320, 48, LoadTexture("nep.png")};
    planets[4] = (Planet){65, 470, 52, LoadTexture("uranus.png")};
    planets[5] = (Planet){500, 500, 50, LoadTexture("venus.png")};

    Ball ball = {460.0f, 450.0f, 14.0f, 0.0f, 0.0f};

    Flipper leftFlipper = {
        {SCREEN_WIDTH / 2.0f - 100.0f, SCREEN_HEIGHT - 150.0f}, 
        80.0f, 15.0f,
        15.0f * DEG2RAD, 15.0f * DEG2RAD, -45.0f * DEG2RAD, 
        480.0f, true, LIGHTGRAY
    };
    
    Flipper rightFlipper = {
        {SCREEN_WIDTH / 2.0f + 100.0f, SCREEN_HEIGHT - 150.0f}, 
        80.0f, 15.0f,
        165.0f * DEG2RAD, 165.0f * DEG2RAD, 225.0f * DEG2RAD, 
        480.0f, false, LIGHTGRAY
    };

    int score = 0;
    bool playCollisionSound = false;

    const float GRAVITY_ACCELERATION = 1200.0f;
    const float WALL_BOUNCE_FACTOR = 0.7f;
    const float PLANET_BOUNCE_FACTOR = 0.85f;
    const float FLIPPER_BOUNCE_FACTOR = 0.90f;
    const float FLIPPER_IMPULSE_STRENGTH = 280.0f;
    const float FLIPPER_VELOCITY_TRANSFER = 0.5f;
    const int PHYSICS_SUBSTEPS = 6;

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        playCollisionSound = false;
        
        bool leftFlipperActive = IsKeyDown(KEY_LEFT);
        bool rightFlipperActive = IsKeyDown(KEY_RIGHT);

        float leftTargetAngle = leftFlipperActive ? leftFlipper.activeAngle : leftFlipper.restingAngle;
        float rightTargetAngle = rightFlipperActive ? rightFlipper.activeAngle : rightFlipper.restingAngle;
        
        float rotationSpeedRad = leftFlipper.rotationSpeedDeg * DEG2RAD;
        
        if (leftFlipper.currentAngle < leftTargetAngle) {
            leftFlipper.currentAngle += rotationSpeedRad * frameTime;
            if (leftFlipper.currentAngle > leftTargetAngle) leftFlipper.currentAngle = leftTargetAngle;
        } else if (leftFlipper.currentAngle > leftTargetAngle) {
            leftFlipper.currentAngle -= rotationSpeedRad * frameTime;
            if (leftFlipper.currentAngle < leftTargetAngle) leftFlipper.currentAngle = leftTargetAngle;
        }
        
        if (rightFlipper.currentAngle < rightTargetAngle) {
            rightFlipper.currentAngle += rotationSpeedRad * frameTime;
            if (rightFlipper.currentAngle > rightTargetAngle) rightFlipper.currentAngle = rightTargetAngle;
        } else if (rightFlipper.currentAngle > rightTargetAngle) {
            rightFlipper.currentAngle -= rotationSpeedRad * frameTime;
            if (rightFlipper.currentAngle < rightTargetAngle) rightFlipper.currentAngle = rightTargetAngle;
        }

        for (int substep = 0; substep < PHYSICS_SUBSTEPS; substep++) {
            float substepDeltaTime = frameTime / (float)PHYSICS_SUBSTEPS;

            ball.velocityY += GRAVITY_ACCELERATION * substepDeltaTime;
            ball.x += ball.velocityX * substepDeltaTime;
            ball.y += ball.velocityY * substepDeltaTime;

            if (ball.x - ball.radius < 0) { 
                ball.x = ball.radius; 
                ball.velocityX *= -WALL_BOUNCE_FACTOR; 
            }
            if (ball.x + ball.radius > SCREEN_WIDTH) { 
                ball.x = SCREEN_WIDTH - ball.radius; 
                ball.velocityX *= -WALL_BOUNCE_FACTOR; 
            }
            if (ball.y - ball.radius < 0) { 
                ball.y = ball.radius; 
                ball.velocityY *= -WALL_BOUNCE_FACTOR; 
            }
            
            if (ball.y > SCREEN_HEIGHT + 100) { 
                ball.x = 300; 
                ball.y = 450; 
                ball.velocityX = 0; 
                ball.velocityY = 0; 
                score = 0; 
            }

            float boundaryYCenter = leftFlipper.pivotPoint.y - 35.0f;
            float boundarySlope = 20.0f;
            Vec2f leftBoundaryStart = {0, boundaryYCenter - boundarySlope};
            Vec2f leftBoundaryEnd = {leftFlipper.pivotPoint.x, boundaryYCenter + boundarySlope};
            Vec2f rightBoundaryStart = {rightFlipper.pivotPoint.x, boundaryYCenter + boundarySlope};
            Vec2f rightBoundaryEnd = {(float)SCREEN_WIDTH, boundaryYCenter - boundarySlope};
            
            SeparateCircleFromSegment(&ball, leftBoundaryStart, leftBoundaryEnd);
            SeparateCircleFromSegment(&ball, rightBoundaryStart, rightBoundaryEnd);

            Flipper flippers[2] = {leftFlipper, rightFlipper};
            bool flipperActive[2] = {leftFlipperActive, rightFlipperActive};
            int flipperBaseScore[2] = {10, 15};
            
            for (int flipperIndex = 0; flipperIndex < 2; flipperIndex++) {
                Flipper *currentFlipper = &flippers[flipperIndex];
                bool hasCollided = false;
                
                Vec2f flipperStartPos = currentFlipper->pivotPoint;
                Vec2f flipperEndPos = {
                    currentFlipper->pivotPoint.x + currentFlipper->length * cosf(currentFlipper->currentAngle),
                    currentFlipper->pivotPoint.y + currentFlipper->length * sinf(currentFlipper->currentAngle)
                };
                float capRadius = currentFlipper->width * 0.5f;
                
                float deltaXToTip = ball.x - flipperEndPos.x;
                float deltaYToTip = ball.y - flipperEndPos.y;
                float distanceToTip = sqrtf(deltaXToTip * deltaXToTip + deltaYToTip * deltaYToTip);
                
                if (distanceToTip < ball.radius + capRadius && !hasCollided) {
                    if (distanceToTip < 1e-5f) distanceToTip = 1e-5f;
                    float normalX = deltaXToTip / distanceToTip;
                    float normalY = deltaYToTip / distanceToTip;
                    
                    if (flipperActive[flipperIndex]) {
                        ball.velocityX += normalX * FLIPPER_IMPULSE_STRENGTH * substepDeltaTime;
                        ball.velocityY += normalY * FLIPPER_IMPULSE_STRENGTH * substepDeltaTime;
                        
                        float angularDirection = (currentFlipper->currentAngle - currentFlipper->restingAngle) > 0 ? 1.0f : -1.0f;
                        float angularVelocity = angularDirection * currentFlipper->rotationSpeedDeg * DEG2RAD;
                        float tipLinearVelocityX = -angularVelocity * (flipperEndPos.y - currentFlipper->pivotPoint.y);
                        float tipLinearVelocityY = angularVelocity * (flipperEndPos.x - currentFlipper->pivotPoint.x);
                        ball.velocityX += tipLinearVelocityX * FLIPPER_VELOCITY_TRANSFER;
                        ball.velocityY += tipLinearVelocityY * FLIPPER_VELOCITY_TRANSFER;
                    } else {
                        ReflectVelocity(&ball, normalX, normalY, FLIPPER_BOUNCE_FACTOR);
                    }
                    
                    ball.x = flipperEndPos.x + normalX * (ball.radius + capRadius);
                    ball.y = flipperEndPos.y + normalY * (ball.radius + capRadius);
                    score += flipperBaseScore[flipperIndex];
                    hasCollided = true;
                    playCollisionSound = true;
                }
                
                if (!hasCollided && CircleSegmentCollision(flipperStartPos, flipperEndPos, &ball)) {
                    Vec2f closestPoint;
                    float segmentParameter = ClosestPointOnSegment(flipperStartPos, flipperEndPos, 
                                                                   (Vec2f){ball.x, ball.y}, &closestPoint);
                    
                    float normalX = ball.x - closestPoint.x;
                    float normalY = ball.y - closestPoint.y;
                    float normalDistance = sqrtf(normalX * normalX + normalY * normalY);
                    if (normalDistance < 1e-5f) normalDistance = 1e-5f;
                    normalX /= normalDistance; 
                    normalY /= normalDistance;
                    
                    if (flipperActive[flipperIndex]) {
                        ball.velocityX += normalX * FLIPPER_IMPULSE_STRENGTH * substepDeltaTime;
                        ball.velocityY += normalY * FLIPPER_IMPULSE_STRENGTH * substepDeltaTime;
                        
                        float angularDirection = (currentFlipper->currentAngle - currentFlipper->restingAngle) > 0 ? 1.0f : -1.0f;
                        float angularVelocity = angularDirection * currentFlipper->rotationSpeedDeg * DEG2RAD;
                        float contactLinearVelocityX = -angularVelocity * (closestPoint.y - currentFlipper->pivotPoint.y);
                        float contactLinearVelocityY = angularVelocity * (closestPoint.x - currentFlipper->pivotPoint.x);
                        ball.velocityX += contactLinearVelocityX * FLIPPER_VELOCITY_TRANSFER;
                        ball.velocityY += contactLinearVelocityY * FLIPPER_VELOCITY_TRANSFER;
                    } else {
                        ReflectVelocity(&ball, normalX, normalY, FLIPPER_BOUNCE_FACTOR);
                    }
                    
                    ball.x = closestPoint.x + normalX * ball.radius;
                    ball.y = closestPoint.y + normalY * ball.radius;
                    score += (int)(flipperBaseScore[flipperIndex] * (0.5f + segmentParameter * 0.5f));
                    hasCollided = true;
                    playCollisionSound = true;
                }
                
                if (!hasCollided) {
                    float deltaXToPivot = ball.x - flipperStartPos.x;
                    float deltaYToPivot = ball.y - flipperStartPos.y;
                    float distanceToPivot = sqrtf(deltaXToPivot * deltaXToPivot + deltaYToPivot * deltaYToPivot);
                    
                    if (distanceToPivot < ball.radius + capRadius) {
                        if (distanceToPivot < 1e-5f) distanceToPivot = 1e-5f;
                        float normalX = deltaXToPivot / distanceToPivot;
                        float normalY = deltaYToPivot / distanceToPivot;
                        
                        if (flipperActive[flipperIndex]) {
                            ball.velocityX += normalX * FLIPPER_IMPULSE_STRENGTH * substepDeltaTime;
                            ball.velocityY += normalY * FLIPPER_IMPULSE_STRENGTH * substepDeltaTime;
                        } else {
                            ReflectVelocity(&ball, normalX, normalY, FLIPPER_BOUNCE_FACTOR);
                        }
                        
                        ball.x = flipperStartPos.x + normalX * (ball.radius + capRadius);
                        ball.y = flipperStartPos.y + normalY * (ball.radius + capRadius);
                        score += flipperBaseScore[flipperIndex] / 2;
                        hasCollided = true;
                        playCollisionSound = true;
                    }
                }
            }

            for (int planetIndex = 0; planetIndex < PLANET_COUNT; planetIndex++) {
                float deltaX = ball.x - planets[planetIndex].x;
                float deltaY = ball.y - planets[planetIndex].y;
                float distance = sqrtf(deltaX * deltaX + deltaY * deltaY);
                float minimumDistance = ball.radius + planets[planetIndex].radius;
                
                if (distance < minimumDistance && distance > 1e-5f) {
                    float normalX = deltaX / distance;
                    float normalY = deltaY / distance;
                    
                    ReflectVelocity(&ball, normalX, normalY, PLANET_BOUNCE_FACTOR);
                    
                    ball.x = planets[planetIndex].x + normalX * minimumDistance;
                    ball.y = planets[planetIndex].y + normalY * minimumDistance;
                    score += 5;
                    playCollisionSound = true;
                }
            }
        }

        if (playCollisionSound) {
            PlaySound(collisionSound);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        if (background.id != 0) DrawTexture(background, 0, 0, WHITE);

        for (int i = 0; i < PLANET_COUNT; i++) {
            if (planets[i].texture.id != 0) {
                Rectangle sourceRect = {0, 0, (float)planets[i].texture.width, (float)planets[i].texture.height};
                Rectangle destRect = {
                    planets[i].x - planets[i].radius, 
                    planets[i].y - planets[i].radius, 
                    planets[i].radius * 2, 
                    planets[i].radius * 2
                };
                DrawTexturePro(planets[i].texture, sourceRect, destRect, (Vector2){0, 0}, 0, WHITE);
            } else {
                DrawCircle((int)planets[i].x, (int)planets[i].y, planets[i].radius, DARKBLUE);
            }
        }

        float boundaryYDraw = leftFlipper.pivotPoint.y - 35.0f;
        float slopeDraw = 20.0f;
        DrawLineEx((Vector2){0, boundaryYDraw - slopeDraw}, 
                   (Vector2){leftFlipper.pivotPoint.x + 5, boundaryYDraw + slopeDraw}, 6, WHITE);
        DrawLineEx((Vector2){rightFlipper.pivotPoint.x - 5, boundaryYDraw + slopeDraw}, 
                   (Vector2){(float)SCREEN_WIDTH, boundaryYDraw - slopeDraw}, 6, WHITE);

        DrawFlipper(&leftFlipper);
        DrawFlipper(&rightFlipper);

        DrawCircleV((Vector2){ball.x, ball.y}, ball.radius, WHITE);
        
        DrawText(TextFormat("Score: %d", score), 10, 10, 24, RAYWHITE);
        
        EndDrawing();
    }

    if (background.id != 0) UnloadTexture(background);
    for (int i = 0; i < PLANET_COUNT; i++) {
        if (planets[i].texture.id != 0) UnloadTexture(planets[i].texture);
    }
    
    UnloadSound(collisionSound);
    CloseAudioDevice();
    CloseWindow();
    return 0;

}
