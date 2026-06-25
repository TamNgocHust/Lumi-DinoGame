#include "stm32f401re_rcc.h"
#include "stm32f401re_gpio.h"
#include "timer.h"
#include "ucg.h"
#include "Ucglib.h"
#include "buzzer.h"
#include "led.h"
#include "button.h"

#define SW          128
#define SH          128
#define GROUND_Y    100
#define DINO_W       14
#define DINO_H       16
#define DINO_X       15
#define DINO_GY     (GROUND_Y - DINO_H)
#define CACT_W       10
#define CACT_H       14
#define CACT_GY     (GROUND_Y - CACT_H)
#define JUMP_H       35
#define JUMP_FR      14
#define FRAME_MS     60
#define CACT_SPD_SLOW 6
#define CACT_SPD_FAST 9
#define MENU_ITEMS    2

#define MENU_Y0     52
#define MENU_Y1     68

typedef enum { ST_MENU, ST_PLAY, ST_OVER } State;

static ucg_t ucg;
static State    state = ST_MENU;
static uint8_t  menuSel = 0;
static uint8_t  prevMenuSel = 0;
static uint8_t  speedFast = 0;
static uint16_t score = 0;
static uint16_t hiScore = 0;
static uint16_t prevScore = 0xFFFF;
static int16_t  dinoY, prevDinoY;
static uint8_t  jumping = 0, jumpFr = 0;
static int16_t  cactX, prevCactX;
static uint8_t  cactSpeed = CACT_SPD_SLOW;
static uint8_t  cactType = 0;
static uint32_t lastFrMs;
static uint32_t overTime;
static uint8_t  pB1 = 1, pB3 = 1, pB5 = 1;
static uint8_t  buzzOn = 0;
static uint32_t buzzOffT = 0;

static void PrepText(void) {
    ucg_SetFont(&ucg, ucg_font_helvR12_tr);
    ucg_SetFontMode(&ucg, UCG_FONT_MODE_SOLID);
    ucg_SetColor(&ucg, 1, 0, 0, 0);
}

static void ClearScreenOnce(void) {
    ucg_ClearScreen(&ucg);
    ucg_SetRotate180(&ucg);

    ucg_SetColor(&ucg, 0, 0, 0, 0);
    ucg_DrawBox(&ucg, 0, 0, SW, SH);
}

static void DrawNumber(int16_t x, int16_t y, uint16_t val) {
    char c;
    uint8_t started = 0;

    if (val >= 100) {
        c = '0';
        while (val >= 100) {
            c++;
            val -= 100;
        }
        ucg_DrawGlyph(&ucg, x, y, 0, c);
        x += 8;
        started = 1;
    }

    if (val >= 10 || started) {
        c = '0';
        while (val >= 10) {
            c++;
            val -= 10;
        }
        ucg_DrawGlyph(&ucg, x, y, 0, c);
        x += 8;
    }

    c = '0';
    while (val >= 1) {
        c++;
        val -= 1;
    }
    ucg_DrawGlyph(&ucg, x, y, 0, c);
}

static void DrawLabel(int16_t x, int16_t y, const char *str) {
    while (*str) {
        x += ucg_DrawGlyph(&ucg, x, y, 0, *str);
        str++;
    }
}

static void DrawPlayHud(void) {
    PrepText();

    ucg_SetColor(&ucg, 0, 90, 90, 90);
    DrawLabel(2, 12, speedFast ? "FAST" : "SLOW");

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawNumber(100, 12, score);
}
static void UpdateScore(void) {
    ucg_SetColor(&ucg, 0, 0, 0, 0);

    // Clear only the score area
    ucg_DrawBox(&ucg, 96, 0, 32, 18);

    PrepText();

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawNumber(100, 12, score);
}

static uint8_t B1(void) {
    uint8_t c = Button_GetLogicInputPin(BUTTON_KIT_ID1);
    uint8_t r = (c == 0 && pB1 == 1);
    pB1 = c;
    return r;
}

static uint8_t B3(void) {
    uint8_t c = Button_GetLogicInputPin(BUTTON_KIT_ID3);
    uint8_t r = (c == 0 && pB3 == 1);
    pB3 = c;
    return r;
}

static uint8_t B5(void) {
    uint8_t c = Button_GetLogicInputPin(BUTTON_KIT_ID5);
    uint8_t r = (c == 0 && pB5 == 1);
    pB5 = c;
    return r;
}

static void DrawDinoFrame(int16_t oldY, int16_t newY) {
    if (oldY != newY) {
        ucg_SetColor(&ucg, 0, 0, 0, 0);

        if (newY < oldY) {
            int16_t h = (oldY + DINO_H) - (newY + DINO_H);
            if (h > 0) {
                ucg_DrawBox(&ucg, DINO_X, newY + DINO_H, DINO_W, h);
            }
        } else {
            int16_t h = newY - oldY;
            if (h > 0) {
                ucg_DrawBox(&ucg, DINO_X, oldY, DINO_W, h);
            }
        }
    }

    int16_t x = DINO_X;
    int16_t y = newY;

    ucg_SetColor(&ucg, 0, 83, 83, 83);
    ucg_DrawBox(&ucg, x + 6, y, 8, 6);

    ucg_SetColor(&ucg, 0, 0, 0, 0);
    ucg_DrawBox(&ucg, x + 8, y + 5, 3, 1);

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    ucg_DrawBox(&ucg, x + 11, y + 1, 2, 2);

    ucg_SetColor(&ucg, 0, 83, 83, 83);
    ucg_DrawBox(&ucg, x + 5, y + 6, 3, 2);
    ucg_DrawBox(&ucg, x + 2, y + 7, 10, 4);
    ucg_DrawBox(&ucg, x + 10, y + 8, 3, 1);
    ucg_DrawBox(&ucg, x + 12, y + 9, 1, 2);
    ucg_DrawBox(&ucg, x + 1, y + 7, 1, 4);
    ucg_DrawBox(&ucg, x, y + 7, 1, 3);
    ucg_DrawBox(&ucg, x + 3, y + 11, 2, 3);
    ucg_DrawBox(&ucg, x + 2, y + 14, 3, 1);
    ucg_DrawBox(&ucg, x + 8, y + 11, 2, 3);
    ucg_DrawBox(&ucg, x + 7, y + 14, 3, 1);
}

static void DrawCact0(int16_t x) {
    ucg_SetColor(&ucg, 0, 50, 140, 50);
    ucg_DrawBox(&ucg, x + 3, CACT_GY, 4, CACT_H);
    ucg_DrawBox(&ucg, x, CACT_GY + 4, 3, 2);
    ucg_DrawBox(&ucg, x, CACT_GY + 2, 2, 2);
    ucg_DrawBox(&ucg, x + 7, CACT_GY + 7, 3, 2);
    ucg_DrawBox(&ucg, x + 8, CACT_GY + 5, 2, 2);
}

static void DrawCact1(int16_t x) {
    ucg_SetColor(&ucg, 0, 40, 130, 40);
    ucg_DrawBox(&ucg, x + 1, CACT_GY + 4, 3, CACT_H - 4);
    ucg_DrawBox(&ucg, x, CACT_GY + 6, 1, 3);
    ucg_DrawBox(&ucg, x + 6, CACT_GY, 3, CACT_H);
    ucg_DrawBox(&ucg, x + 9, CACT_GY + 3, 1, 3);
    ucg_DrawBox(&ucg, x + 4, CACT_GY + 8, 2, 2);
}

static void DrawCact2(int16_t x) {
    ucg_SetColor(&ucg, 0, 60, 150, 60);
    ucg_DrawBox(&ucg, x + 1, CACT_GY + 2, 2, CACT_H - 2);
    ucg_DrawBox(&ucg, x + 4, CACT_GY, 2, CACT_H);
    ucg_DrawBox(&ucg, x + 7, CACT_GY + 3, 2, CACT_H - 3);
    ucg_DrawBox(&ucg, x, CACT_GY + 5, 4, 2);
    ucg_DrawBox(&ucg, x + 6, CACT_GY + 6, 4, 2);
}

static void DrawCactFrame(int16_t oldX, int16_t newX) {
    if (oldX != newX && oldX >= -CACT_W && oldX <= SW) {
        ucg_SetColor(&ucg, 0, 0, 0, 0);

        int16_t eX = newX + CACT_W;
        int16_t eW = (oldX + CACT_W) - eX;

        if (eW > 0 && eX >= 0 && eX < SW) {
            ucg_DrawBox(&ucg, eX, CACT_GY, eW, CACT_H);
        }
    }

    if (newX >= -CACT_W && newX <= SW) {
        if (cactType == 0) {
            DrawCact0(newX);
        } else if (cactType == 1) {
            DrawCact1(newX);
        } else {
            DrawCact2(newX);
        }
    }
}

static void EraseCactFull(int16_t x) {
    if (x >= -CACT_W && x <= SW) {
        ucg_SetColor(&ucg, 0, 0, 0, 0);
        ucg_DrawBox(&ucg, x, CACT_GY, CACT_W, CACT_H);
    }
}

static void MenuDrawCursor(void) {
    PrepText();

    ucg_SetColor(&ucg, 0, 0, 0, 0);
    ucg_DrawBox(&ucg, 3, (prevMenuSel == 0 ? MENU_Y0 : MENU_Y1) - 12, 12, 15);

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(5, menuSel == 0 ? MENU_Y0 : MENU_Y1, ">");
}

static void MenuDrawSpeed(void) {
    ucg_SetColor(&ucg, 0, 0, 0, 0);
    ucg_DrawBox(&ucg, 16, MENU_Y1 - 14, 112, 18);

    PrepText();

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(18, MENU_Y1, "Speed:");
    DrawLabel(72, MENU_Y1, speedFast ? "Fast" : "Slow");

    if (menuSel == 1) {
        ucg_SetColor(&ucg, 0, 255, 255, 255);
        DrawLabel(5, MENU_Y1, ">");
    }
}

static void MenuFull(void) {
    ClearScreenOnce();

    PrepText();

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(15, 14, "DINO GAME");

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(20, 30, "Best:");
    DrawNumber(58, 30, hiScore);

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(18, MENU_Y0, "Start Game");

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(18, MENU_Y1, "Speed:");
    DrawLabel(72, MENU_Y1, speedFast ? "Fast" : "Slow");

    prevMenuSel = menuSel;

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(5, menuSel == 0 ? MENU_Y0 : MENU_Y1, ">");
}

static void GameStart(void) {
    state = ST_PLAY;
    score = 0;
    prevScore = 0xFFFF;

    dinoY = DINO_GY;
    prevDinoY = DINO_GY;

    jumping = 0;
    jumpFr = 0;

    cactX = SW + 20;
    prevCactX = cactX;

    cactSpeed = speedFast ? CACT_SPD_FAST : CACT_SPD_SLOW;
    cactType = 0;

    lastFrMs = GetMilSecTick();

    ClearScreenOnce();

    DrawPlayHud();

    ucg_SetColor(&ucg, 0, 90, 90, 90);
    ucg_DrawHLine(&ucg, 0, GROUND_Y, SW);

    DrawDinoFrame(DINO_GY, DINO_GY);
}

static void GameOver(void) {
    state = ST_OVER;
    overTime = GetMilSecTick();

    if (score > hiScore) {
        hiScore = score;
    }

    LedControl_SetColorIndividual(LED_KIT_ID0, LED_COLOR_RED, 100);
    BuzzerControl_SetDutyCycle(30);
    buzzOn = 0;

    ucg_SetColor(&ucg, 0, 40, 0, 0);
    ucg_DrawBox(&ucg, 4, 18, 120, 68);

    PrepText();

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(14, 34, "GAME OVER!");

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(18, 52, "Score:");
    DrawNumber(68, 52, score);

    ucg_SetColor(&ucg, 0, 255, 255, 255);
    DrawLabel(25, 68, "Best:");
    DrawNumber(67, 68, hiScore);
}

static uint8_t Hit(void) {
    return (DINO_X < cactX + CACT_W &&
            DINO_X + DINO_W > cactX &&
            dinoY < CACT_GY + CACT_H &&
            dinoY + DINO_H > CACT_GY);
}

static void ClkInit(void) {
    RCC_DeInit();
    RCC_HSICmd(ENABLE);

    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);

    FLASH->ACR = FLASH_ACR_LATENCY_2WS |
                 FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN;

    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div2);
    RCC_PCLK2Config(RCC_HCLK_Div1);

    RCC_PLLConfig(RCC_PLLSource_HSI, 16, 336, 4, 7);
    RCC_PLLCmd(ENABLE);

    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    while (RCC_GetSYSCLKSource() != 0x08);

    SystemCoreClock = 84000000;
}

int main(void) {
    ClkInit();
    TimerInit();
    LedControl_Init();
    BuzzerControl_Init();
    Button_Init();

    Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
    ucg_SetRotate180(&ucg);
    ClearScreenOnce();

    state = ST_MENU;
    MenuFull();

    while (1) {
        processTimerScheduler();

        uint8_t b1 = B1();
        uint8_t b3 = B3();
        uint8_t b5 = B5();

        if (buzzOn && GetMilSecTick() - buzzOffT >= 80) {
            BuzzerControl_SetDutyCycle(0);
            buzzOn = 0;
        }

        if (state == ST_MENU) {
            static uint32_t menuLock = 0;
            uint32_t now = GetMilSecTick();

            if (now - menuLock < 300) {
                continue;
            }

            if (b1 && menuSel > 0) {
                menuLock = now;
                prevMenuSel = menuSel;
                menuSel--;
                MenuDrawCursor();
            } else if (b5 && menuSel < MENU_ITEMS - 1) {
                menuLock = now;
                prevMenuSel = menuSel;
                menuSel++;
                MenuDrawCursor();
            } else if (b3) {
                menuLock = now;

                if (menuSel == 0) {
                    GameStart();
                } else {
                    speedFast ^= 1;
                    MenuDrawSpeed();
                }
            }

            continue;
        }

        if (state == ST_OVER) {
            if (GetMilSecTick() - overTime >= 2000) {
                LedControl_SetColorIndividual(LED_KIT_ID0, LED_COLOR_RED, 0);
                BuzzerControl_SetDutyCycle(0);
                buzzOn = 0;

                state = ST_MENU;
                MenuFull();
            }

            continue;
        }

        if (b3 && !jumping) {
            jumping = 1;
            jumpFr = 0;

            BuzzerControl_SetDutyCycle(50);
            buzzOn = 1;
            buzzOffT = GetMilSecTick();
        }

        uint32_t now = GetMilSecTick();

        if (now - lastFrMs < FRAME_MS) {
            continue;
        }

        lastFrMs = now;

        prevDinoY = dinoY;
        prevCactX = cactX;

        if (jumping) {
            jumpFr++;

            uint8_t half = JUMP_FR / 2;

            if (jumpFr <= half) {
                dinoY = DINO_GY - (JUMP_H * jumpFr) / half;
            } else if (jumpFr <= JUMP_FR) {
                dinoY = (DINO_GY - JUMP_H) + (JUMP_H * (jumpFr - half)) / half;
            } else {
                dinoY = DINO_GY;
                jumping = 0;
                jumpFr = 0;
            }
        }

        cactX -= cactSpeed;

        if (cactX < -CACT_W) {
            EraseCactFull(prevCactX);

            cactX = SW + 10 + (now % 25);
            prevCactX = cactX;

            cactType = (cactType + 1 + (now % 2)) % 3;
            score++;
        }

        if (Hit()) {
            DrawDinoFrame(prevDinoY, dinoY);
            DrawCactFrame(prevCactX, cactX);
            GameOver();
            continue;
        }

        DrawCactFrame(prevCactX, cactX);
        DrawDinoFrame(prevDinoY, dinoY);

        ucg_SetColor(&ucg, 0, 90, 90, 90);
        ucg_DrawHLine(&ucg, 0, GROUND_Y, SW);

        if (score != prevScore) {
            prevScore = score;
            UpdateScore();
        }
    }
}
