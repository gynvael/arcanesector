#include <cassert>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include "aes/aes.hpp"
#include "md5/md5.hpp"
#include "logic.h"
#include "gamestate.h"

void GameLogic::TextInputSubsystem::reset(std::string *s) {
  text = s;
  ctrl = false;
  alt = false;
  shift = false;
  //capslock = false;
  repeated_char = 0;
}

void GameLogic::TextInputSubsystem::handle(EventUIGame *ev) {
  if (text == nullptr) {
    return;
  }

  repeated_char = 0;  // By default stop repeating on an event.

  if (ev->type == EventUIGame::KEY_UP) {
    // Totally ignoring the case where both left and right modifier keys
    // are held, and then only one if let go.
    if (ev->key_code == key_code::CTRL_LEFT ||
        ev->key_code == key_code::CTRL_RIGHT) {
      ctrl = false;
      return;
    }

    if (ev->key_code == key_code::SHIFT_LEFT ||
        ev->key_code == key_code::SHIFT_RIGHT) {
      shift = false;
      return;
    }

    if (ev->key_code == key_code::ALT_LEFT ||
        ev->key_code == key_code::ALT_RIGHT) {
      alt = false;
      return;
    }

    return;
  }

  if (ev->type == EventUIGame::KEY_DOWN) {
    if (ev->key_code == key_code::CTRL_LEFT ||
        ev->key_code == key_code::CTRL_RIGHT) {
      ctrl = true;
      return;
    }

    if (ev->key_code == key_code::SHIFT_LEFT ||
        ev->key_code == key_code::SHIFT_RIGHT) {
      shift = true;
      return;
    }

    if (ev->key_code == key_code::ALT_LEFT ||
        ev->key_code == key_code::ALT_RIGHT) {
      alt = true;
      return;
    }

    if (ev->key_code == key_code::CAPSLOCK) {
      capslock = !capslock;
      return;
    }

    bool shifted = (capslock && !shift) || (shift && !capslock);

    switch (ev->key_code) {
      case key_code::NUM_0: repeated_char = "0)"[shifted]; break;
      case key_code::NUM_1: repeated_char = "1!"[shifted]; break;
      case key_code::NUM_2: repeated_char = "2@"[shifted]; break;
      case key_code::NUM_3: repeated_char = "3#"[shifted]; break;
      case key_code::NUM_4: repeated_char = "4$"[shifted]; break;
      case key_code::NUM_5: repeated_char = "5%"[shifted]; break;
      case key_code::NUM_6: repeated_char = "6^"[shifted]; break;
      case key_code::NUM_7: repeated_char = "7&"[shifted]; break;
      case key_code::NUM_8: repeated_char = "8*"[shifted]; break;
      case key_code::NUM_9: repeated_char = "9("[shifted]; break;
      case key_code::A:     repeated_char = "aA"[shifted]; break;
      case key_code::B:     repeated_char = "bB"[shifted]; break;
      case key_code::C:     repeated_char = "cC"[shifted]; break;
      case key_code::D:     repeated_char = "dD"[shifted]; break;
      case key_code::E:     repeated_char = "eE"[shifted]; break;
      case key_code::F:     repeated_char = "fF"[shifted]; break;
      case key_code::G:     repeated_char = "gG"[shifted]; break;
      case key_code::H:     repeated_char = "hH"[shifted]; break;
      case key_code::I:     repeated_char = "iI"[shifted]; break;
      case key_code::J:     repeated_char = "jJ"[shifted]; break;
      case key_code::K:     repeated_char = "kK"[shifted]; break;
      case key_code::L:     repeated_char = "lL"[shifted]; break;
      case key_code::M:     repeated_char = "mM"[shifted]; break;
      case key_code::N:     repeated_char = "nN"[shifted]; break;
      case key_code::O:     repeated_char = "oO"[shifted]; break;
      case key_code::P:     repeated_char = "pP"[shifted]; break;
      case key_code::Q:     repeated_char = "qQ"[shifted]; break;
      case key_code::R:     repeated_char = "rR"[shifted]; break;
      case key_code::S:     repeated_char = "sS"[shifted]; break;
      case key_code::T:     repeated_char = "tT"[shifted]; break;
      case key_code::U:     repeated_char = "uU"[shifted]; break;
      case key_code::V:     repeated_char = "vV"[shifted]; break;
      case key_code::W:     repeated_char = "wW"[shifted]; break;
      case key_code::X:     repeated_char = "xX"[shifted]; break;
      case key_code::Y:     repeated_char = "yY"[shifted]; break;
      case key_code::Z:     repeated_char = "zZ"[shifted]; break;
      case key_code::QUOTE: repeated_char = "'\""[shifted]; break;
      case key_code::COMMA: repeated_char = ",<"[shifted]; break;
      case key_code::MINUS: repeated_char = "-_"[shifted]; break;
      case key_code::PERIOD: repeated_char = ".>"[shifted]; break;
      case key_code::SLASH: repeated_char = "/?"[shifted]; break;
      case key_code::SEMICOLON: repeated_char = ";:"[shifted]; break;
      case key_code::EQUALS: repeated_char = "=+"[shifted]; break;
      case key_code::SQUARE_LEFT: repeated_char = "[{"[shifted]; break;
      case key_code::SQUARE_RIGHT: repeated_char = "]}"[shifted]; break;
      case key_code::BACKSLASH: repeated_char = "\\|"[shifted]; break;
      case key_code::BACKTICK: repeated_char = "`~"[shifted]; break;

      case key_code::SPACE: repeated_char = ' '; break;
      case key_code::BACKSPACE: repeated_char = '\b'; break;

      default: break;
    }

    if (repeated_char != 0) {
      emit();
      repeated_fast = false;
    }
  }
}

void GameLogic::TextInputSubsystem::emit() {
  if (text == nullptr || repeated_char == 0) {
    return;
  }

  last_emit = std::chrono::steady_clock::now();

  if (repeated_char == '\b') {
    if (!text->empty()) {
      text->pop_back();
    }
  } else {
    (*text) += repeated_char;
  }
}

void GameLogic::TextInputSubsystem::handle_repeats() {
  if (text == nullptr || repeated_char == 0) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  const float diff = (now - last_emit).count();

  if (diff >= (repeated_fast ? REPEAT_COOLDOWN : REPEAT_INITIAL_DELAY)) {
    emit();
    repeated_fast = true;
  }
}

bool GameLogic::process_net_event() {
  EventNetGame ev;
  if (!ctx->queue_net_from->pop(&ev)) {
    return false;
  }

  if (ev.type == EventNetGame::DISCONNECT) {
    puts("TODO: disconnects/reconnects");
    ctx->end = true;
    return true;
  }

  assert(ev.type == EventNetGame::PACKET);

  std::unique_ptr<PacketsSC> p{ev.packet};

  if (p->get_chunk_id() == "GRND"s) {
    state.ground_items.clear();
    PacketsSC_GRND *grnd = (PacketsSC_GRND*)p.get();
    for (const auto& itemlist : grnd->lists) {
      for (const auto& item : itemlist.items) {
        state.item_id_to_item[item.id] = item;
      }

      uint64_t key = GameState::coords_to_key(itemlist.pos_x, itemlist.pos_y);
      state.ground_items[key] = std::move(itemlist.items);
    }
    return true;
  }

  if (p->get_chunk_id() == "MOBS"s) {
    state.ground_mobs.clear();
    PacketsSC_MOBS *mobs = (PacketsSC_MOBS*)p.get();
    for (const auto& mob : mobs->moblist) {
      state.mob_id_to_mob[mob.id] = mob;

      if (mob.visible) {
        uint64_t key = GameState::coords_to_key(mob.pos_x, mob.pos_y);
        state.ground_mobs[key].push_back(mob);
      }
    }
    return true;
  }

  if (p->get_chunk_id() == "POSI"s) {
    PacketsSC_POSI *posi = (PacketsSC_POSI*)p.get();
    /*printf("POSI: %i, %i (dir: %c)\n",
        posi->pos_x, posi->pos_y,
        posi->direction >= 4 ? '?' : "NSWE"[posi->direction]);*/

    state.player_x = posi->pos_x;
    state.player_y = posi->pos_y;
    state.player_dir = posi->direction;
    return true;
  }


  // Check if we got NOPC (a request for PC creation).
  if (p->get_chunk_id() == "NOPC"s) {
    const char *pc_name = getenv("ARCANE_NAME");
    if (pc_name == nullptr) {
      pc_name = "ZeroCool1337";
    }

    auto p = PacketsCS_MYPC::make(pc_name, 1);
    ctx->queue_net_to->push(
        EventGameNet{EventGameNet::PACKET, p.release()});
    return true;
  }

  if (p->get_chunk_id() == "GAME"s) {
    puts("Alright! We can play the game!");
    return true;
  }

  if (p->get_chunk_id() == "INFO"s) {
    PacketsSC_INFO *info = (PacketsSC_INFO*)p.get();
    state.player_hp = info->hp;
    state.player_hp_max = info->hp_max;
    state.player_mana = info->mana;
    state.player_mana_max = info->mana_max;
    if (!info->name.empty()) {
      state.player_name = info->name;
    }
    return true;
  }

  if (p->get_chunk_id() == "INVT"s) {
    PacketsSC_INVT *invt = (PacketsSC_INVT*)p.get();
    /*printf("INVT:\n"
           "  Inventory:\n");*/

    for (int i = 0; i < 8; i++) {
      const auto& item = invt->inventory[i];
      state.inventory[i] = item;
      state.item_id_to_item[item.id] = item;

      // Debug:
      /*printf("    [%i] %.16llx: ", i, item.id);
      if (item.id != ITEM_NON_EXISTING_ID) {
        printf("[%s] %s (%s)",
          item.movable ? "movable" : "immutable",
          item.name.c_str(),
          item.gfx_id.c_str());
      }*/
      //putchar('\n');
    }

    //printf("  Equipment:\n");
    for (int i = 0; i < 2; i++) {
      const auto& item = invt->equipment[i];
      state.equiped[i] = item;
      state.item_id_to_item[item.id] = item;

      // Debug:
      /*printf("    [%i] %.16llx: ", i, item.id);
      if (item.id != ITEM_NON_EXISTING_ID) {
        printf("[%s] %s (%s)",
          item.movable ? "movable" : "immutable",
          item.name.c_str(),
          item.gfx_id.c_str());
      }*/
      //putchar('\n');
    }

    return true;
  }

  if (p->get_chunk_id() == "TEXT"s) {
    PacketsSC_TEXT *text = (PacketsSC_TEXT*)p.get();
    ctx->e->in_game_text.puts(text->text);
    return true;
  }

  if (p->get_chunk_id() == "PONG"s) {
    // TODO: measure roundtrip maybe?
    return true;
  }

  if (p->get_chunk_id() == "SLCT"s) {
    state.selecting = true;
    state.selecting_packet_id = p->h.packet_id;
    auto img = ctx->e->img.get("ui_select_cursor");
    int hx = img->w / 2;
    int hy = img->h / 2;
    ctx->queue_ui_to->push(
        EventGameUI{EventGameUI::CURSOR, img, nullptr, hx, hy});
    return true;
  }

  if (p->get_chunk_id() == "HLDI"s) {
    PacketsSC_HLDI *hldi = (PacketsSC_HLDI*)p.get();
    state.holding = hldi->item;
    if (state.holding.id != ITEM_NON_EXISTING_ID) {
      auto img = ctx->e->img.get(hldi->item.gfx_id);
      int hx = img->w / 2;
      int hy = img->h / 2;
      ctx->queue_ui_to->push(
        EventGameUI{EventGameUI::CURSOR, img, nullptr, hx, hy});
    } else {
      ctx->queue_ui_to->push(
        EventGameUI{EventGameUI::CURSOR, nullptr});
    }
    return true;
  }

  printf("unhandled packet???: %s\n", p->get_chunk_id().c_str());

  return true;
}

bool GameLogic::process_ui_event_splashscreen(EventUIGame * /*ev*/) {
  return true;
}

bool GameLogic::process_ui_event_console(EventUIGame *ev) {
  if (ev->type == EventUIGame::KEY_DOWN &&
      ev->key_code == key_code::ESCAPE) {
    text_input.reset(nullptr);
    scene_switch(GameState::SCENE_GAME);
    return true;
  }

  if (ev->type == EventUIGame::KEY_DOWN &&
      ev->key_code == key_code::ENTER) {
    std::string& input_text = ctx->e->debug_con.input_text();

    if (!input_text.empty()) {
      if (input_text.size() > 256) {
        input_text.resize(256);
      }

      std::string text;
      text += "\x13> \x12";
      text += input_text;
      ctx->e->debug_con.puts(text);

      process_console_command(input_text);
      input_text.clear();
    } else {
      ctx->e->debug_con.puts("\x13> \x12");
    }

    return true;
  }

  text_input.handle(ev);
  return true;
}

bool GameLogic::process_ui_event_game_chat(EventUIGame *ev) {
  if (ev->type == EventUIGame::KEY_DOWN &&
      ev->key_code == key_code::ENTER) {
    // Push text message to the server (if any).
    std::string& input_text = ctx->e->in_game_text.input_text();

    if (!input_text.empty()) {
      if (input_text.size() > 256) {
        input_text.resize(256);
      }

      ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_SAYS::make(input_text).release()});

      std::string text;
      text += "\x13You said: \x12";
      text += input_text;
      ctx->e->in_game_text.puts(text);
    }

    text_input.reset(nullptr);

    // Switch back to game.
    game_input_switch(GameState::INPUT_GAME);
    return true;
  }

  text_input.handle(ev);
  return true;
}

bool GameLogic::process_ui_event_game(EventUIGame *ev) {
  // If in chat mode handle keyboard events differently (but mouse handling
  // stays the same).
  if (state.game_input == GameState::INPUT_CHAT && (
      ev->type == EventUIGame::KEY_UP ||
      ev->type == EventUIGame::KEY_DOWN)) {
    return process_ui_event_game_chat(ev);
  }

  if (ev->type == EventUIGame::MOUSE_MOVE) {
    state.mx = ev->mx;
    state.my = ev->my;
    return true;
  }

  if (ev->type == EventUIGame::MOUSE_DOWN) {
    state.mx = ev->mx;
    state.my = ev->my;

    if (ev->mouse_button == mouse_button::LEFT) {
      state.mleft = true;
      state.mx_down_left = ev->mx;
      state.my_down_left = ev->my;
    } else if (ev->mouse_button == mouse_button::RIGHT) {
      state.mright = true;
      state.mx_down_right = ev->mx;
      state.my_down_right = ev->my;
    }

    return true;
  }

  if (ev->type == EventUIGame::MOUSE_UP) {
    state.mx = ev->mx;
    state.my = ev->my;

    if (ev->mouse_button == mouse_button::LEFT) {
      state.mleft = false;
    } else if (ev->mouse_button == mouse_button::RIGHT) {
      state.mright = false;
    }

    return true;
  }


  // Handle normal game input.
  switch (ev->type) {
    case EventUIGame::KEY_UP: {
      // TODO: move this to KEYPRESS event or sth.
      // TODO: strafing
      if (ev->key_code == key_code::ARROW_UP ||
          ev->key_code == key_code::W) {
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_MOVE::make(MOVE_FORWARD).release()});
        break;
      }

      if (ev->key_code == key_code::ARROW_DOWN ||
          ev->key_code == key_code::S) {
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_MOVE::make(MOVE_BACKWARD).release()});
        break;
      }

      if (ev->key_code == key_code::ARROW_LEFT ||
          ev->key_code == key_code::A) {
        const uint8_t dir[] = { WEST, EAST, SOUTH, NORTH };
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_DIRE::make(dir[state.player_dir]).release()});
        break;
      }

      if (ev->key_code == key_code::ARROW_RIGHT ||
          ev->key_code == key_code::D) {
        const uint8_t dir[] = { EAST, WEST, NORTH, SOUTH };
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_DIRE::make(dir[state.player_dir]).release()});
        break;
      }
    }
    break;

    case EventUIGame::KEY_DOWN: {
      // TODO: here and in other places - check if the compiler makes a switch
      // out of this while optimizing (it should).
      if (ev->key_code == key_code::ENTER) {
        game_input_switch(GameState::INPUT_CHAT);
        break;
      }

      if (ev->key_code == key_code::BACKTICK) {
        scene_switch(GameState::SCENE_CONSOLE);
        break;
      }

      if (ev->key_code == key_code::TAB) {
        if (state.game_hud == GameState::HUD_INVENTORY) {
          state.game_hud = GameState::HUD_SPELL;
        } else {
          state.game_hud = GameState::HUD_INVENTORY;
        }
        break;
      }

      if (ev->key_code == key_code::SPACE) {
        cast_spell();
      }

      if (ev->key_code == key_code::ESCAPE &&
          state.game_hud == GameState::HUD_SPELL) {
        state.spell_length = 0;
      }

    }
    break;

    default: break;
  }

  return true;
}

void GameLogic::cast_spell() {
  if (state.spell_length == 0) {
    return;
  }

  // Zero-out remaining runes.
  for (int i = state.spell_length; i < 8; i++) {
    state.spell[i] = 0;
  }

  // Send.
  ctx->queue_net_to->push(EventGameNet{
      EventGameNet::PACKET,
      PacketsCS_CAST::make(state.spell).release()});

  // Reset spell.
  state.spell_length = 0;
}

bool GameLogic::process_ui_event() {
  // Handle text repeats if needed.
  //text_input.handle_repeats();  //  Seems the UI libraries handle this ^_-.

  // Handle actual UI events.
  EventUIGame ev;
  if (!ctx->queue_ui_from->pop(&ev)) {
    return false;
  }

  switch (ev.type) {
    case EventUIGame::REQUEST_FRAME: {
      // Reset action items.
      state.clicked_spell_cast = false;
      state.wants_to_hold_item = ITEM_NON_EXISTING_ID;
      state.wants_to_use_item = ITEM_NON_EXISTING_ID;
      state.wants_to_drop_item = false;
      state.item_drop_dst = 255;
      state.wants_to_select = false;
      state.selection_target_type = 255;
      state.selection_target = 0;

      // Render the frame (and process certain interactive events).
      ctx->e->render_frame(&state);
      ctx->queue_ui_to->push(
        EventGameUI{EventGameUI::FRAME, &ctx->e->c});

      // Check on action items.
      if (state.clicked_spell_cast) {
        cast_spell();
      }

      if (state.wants_to_hold_item != ITEM_NON_EXISTING_ID) {
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_HOLD::make(state.wants_to_hold_item).release()});
      }

      if (state.wants_to_use_item != ITEM_NON_EXISTING_ID) {
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_USEI::make(state.wants_to_use_item).release()});
      }

      if (state.wants_to_drop_item) {
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_DROP::make(state.item_drop_dst).release()});

        // Actually stop holding as far as engine is concerned.
        state.holding.id = ITEM_NON_EXISTING_ID;
      }

      if (state.wants_to_select) {
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_THIS::make(
              state.selecting_packet_id,
              state.selection_target_type,
              state.selection_target).release()});

        // Selecting is done as far as client is concerned.
        state.selecting = false;
        ctx->queue_ui_to->push(
            EventGameUI{EventGameUI::CURSOR, nullptr});
      }
    }
    break;

    case EventUIGame::KEY_UP:
    case EventUIGame::KEY_DOWN:
    case EventUIGame::MOUSE_DOWN:
    case EventUIGame::MOUSE_UP:
    case EventUIGame::MOUSE_MOVE: {
      switch (state.game_scene) {
        case GameState::SCENE_SPLASHSCREEN:
          return process_ui_event_splashscreen(&ev);

        case GameState::SCENE_GAME:
          return process_ui_event_game(&ev);

        case GameState::SCENE_CONSOLE:
          return process_ui_event_console(&ev);
      }
    }
    break;

    case EventUIGame::EXIT: {
      ctx->end = true;
    }
    break;
  }

  return true;
}

// TODO: Move this to some common/utility file.
std::vector<std::string> GameLogic::split_string(const std::string& t) {
  std::vector<std::string> v;
  std::istringstream iss(t);
  std::string s;
  while (std::getline(iss, s, ' ')) {
    if (!s.empty()) {
      v.push_back(s);
    }
  }
  return v;
}

void GameLogic::process_console_command(const std::string& t) {
  if (t.empty()) {
    return;
  }

  auto args = split_string(t);
  if (args.empty()) {
    return;
  }

  std::string command = args[0];
  args.erase(args.begin());  // Pop-front.

  if (console_commands.find(command) == console_commands.end()) {
    ctx->e->debug_con.puts("Type 'help' for help.");
    return;
  }

  (this->*console_commands[command])(command, args);
}

void GameLogic::scene_switch(GameState::game_scene_t scene) {
  switch (scene) {
    case GameState::SCENE_SPLASHSCREEN:
      state.game_scene = GameState::SCENE_SPLASHSCREEN;
      break;

    case GameState::SCENE_GAME:
      state.game_scene = GameState::SCENE_GAME;
      break;

    case GameState::SCENE_CONSOLE:
      state.game_scene = GameState::SCENE_CONSOLE;
      ctx->e->debug_con.set_prompt("> ", true);
      text_input.reset(&ctx->e->debug_con.input_text());
      break;
  }
}

void GameLogic::game_input_switch(GameState::game_input_t input) {
  switch (input) {
    case GameState::INPUT_CHAT:
      state.game_input = GameState::INPUT_CHAT;
      ctx->e->in_game_text.input_text().clear();
      ctx->e->in_game_text.set_prompt("say> ", true);
      text_input.reset(&ctx->e->in_game_text.input_text());
      break;

    case GameState::INPUT_GAME:
      state.game_input = GameState::INPUT_GAME;
      ctx->e->in_game_text.set_prompt("", false);
      break;
  }
}

void GameLogic::console_command_quit(
    std::string /*command*/, std::vector<std::string> /*args*/) {
  ctx->end = true;
}

void GameLogic::console_command_cfg_dump(
    std::string /*command*/, std::vector<std::string> args) {
  if (args.size() != 1) {
    ctx->e->debug_con.puts("Error: Invalid argument count (must be 1).");
    return;
  }

  std::string &fname = args.at(0);
  if (fname.find("..") != std::string::npos ||
      fname.find("\\") != std::string::npos ||
      fname.find("/") != std::string::npos ||
      fname.find(":") != std::string::npos ||
      fname.find("$") != std::string::npos) {
    ctx->e->debug_con.puts("Error: Forbidden substring in file name.");
    return;
  }

  std::string path;
  path += "files/";
  path += fname;

  FILE *f = fopen(path.c_str(), "rb");
  if (f == nullptr) {
    ctx->e->debug_con.puts("Error: File not found or couldn't open it.");
    return;
  }

  std::vector<uint8_t> data;
  fseek(f, 0, SEEK_END);
  size_t sz = ftell(f);
  data.resize(sz);
  fseek(f, 0, SEEK_SET);
  if (!fread(data.data(), 1, sz, f)) {
    ctx->e->debug_con.puts("Real error - tell CTF admin this happend.");
  }
  fclose(f);

  if (console_conf_scheme == CONF_SCHEME_MD5_AES_128_EBC) {
    console_conf_decrypt(data);
  }

  if (data[0] != 'C' ||
      data[1] != 'F' ||
      data[2] != 'G') {
    ctx->e->debug_con.puts(
      "Error: Config file MUST start with 'CFG' magic, but starts with:");
    console_hexii_dump(data.data(), 16);
    return;
  }

  ctx->e->debug_con.puts("Config file:");
  console_hexii_dump(data.data(), data.size());
}

void GameLogic::console_conf_decrypt(std::vector<uint8_t>& data) {
  // Calculate key.
  unsigned char key[16];
  MD5_CTX md5;
  MD5_Init(&md5);
  MD5_Update(&md5, console_conf_password.data(), console_conf_password.size());
  MD5_Final(key, &md5);

  // Pad with zeros to full block size.
  while (data.size() % 16 != 0) {
    data.push_back(0);
  }

  // Decrypt.
  AES_ctx aes;
  AES_init_ctx(&aes, key);

  for (size_t i = 0; i < data.size() / 16; i++) {
    AES_ECB_decrypt(&aes, &data[i * 16]);
  }
}

void GameLogic::console_hexii_dump(uint8_t *data, size_t sz) {
  std::string out;
  char buf[32];  // Helper buf.
  size_t i = 0;
  while (i < sz) {
    if (i % 16 == 0) {
      if (i != 0) {
        out += "\n";
      }

      sprintf(buf, "%.4x: ", (unsigned int)i);
      out += buf;
    }

    /*if (data[i] >= 0x20 && data[i] <= 0x7e) {
      sprintf(buf, "\x1a" ".%c\x0f ", data[i]);
    } else if (data[i] == 0xff) {
      sprintf(buf, "## ");
    } else if (data[i] == 0x00) {
      sprintf(buf, "   ");
    } else {*/
    sprintf(buf, "%.2x ", data[i]);
    //}
    out += buf;

    i++;
  }

  ctx->e->debug_con.puts(out);
}

void GameLogic::console_command_cfg_scheme(
    std::string /*command*/, std::vector<std::string> args) {
  if (args.size() != 1) {
    ctx->e->debug_con.puts("Error: Invalid argument count (must be 1).");
    return;
  }

  std::string &scheme = args.at(0);
  if (scheme == "0") {
    console_conf_scheme = CONF_SCHEME_NONE;
    ctx->e->debug_con.puts("Config decryption set to NONE.");
  } else if (scheme == "1") {
    console_conf_scheme = CONF_SCHEME_MD5_AES_128_EBC;
    ctx->e->debug_con.puts("Config decryption set to MD5_AES_128_ECB.");
  } else {
    ctx->e->debug_con.puts("Error: Invalid argument (has to be either 0 or 1).");
  }
}

void GameLogic::console_command_cfg_passwd(
    std::string /*command*/, std::vector<std::string> args) {
  if (args.size() != 1) {
    ctx->e->debug_con.puts("Error: Invalid argument count (must be 1).");
    return;
  }
  console_conf_password = args.at(0);
  ctx->e->debug_con.puts("Config decryption password set.");
}

void GameLogic::console_command_help(
    std::string /*command*/, std::vector<std::string> /*args*/) {
  ctx->e->debug_con.puts(
      "Available commands:\n"
      "  help                  Print this message." "\n"
      "  cfgdump <fname>      Print content of config file." "\n"
      "  cfgscheme <0|1>      Select crypto scheme for config file." "\n"
      "                        0 - None." "\n"
      "                        1 - AES-128-ECB with MD5(password) as key." "\n"
      "  cfgpasswd <passwd>   Set decryption password for config file." "\n"
      "  quit                  Take a guess."
  );
}

void GameLogic::main(GameThreadContext *ctx) {
  this->ctx = ctx;

  state.player_gender = ctx->config->player_id % 2;

  // Console commands.
  console_commands["help"] = &GameLogic::console_command_help;
  console_commands["quit"] = &GameLogic::console_command_quit;
  console_commands["cfgdump"] = &GameLogic::console_command_cfg_dump;
  console_commands["cfgscheme"] = &GameLogic::console_command_cfg_scheme;
  console_commands["cfgpasswd"] = &GameLogic::console_command_cfg_passwd;

  // Some welcome messages.
  ctx->e->debug_con.puts("\x12""Arcane Sector\x0f"" debug console.");
  ctx->e->debug_con.puts("Type 'help' for help. Press ESC to exit console.");
  ctx->e->in_game_text.puts("[Debug build: Press ` to enter debug console]");

  // Initialize the rest of state.
  state.holding.id = ITEM_NON_EXISTING_ID;
  for (int i = 0; i < 8; i++) {
    state.inventory[i].id = ITEM_NON_EXISTING_ID;
  }
  for (int i = 0; i < 2; i++) {
    state.equiped[i].id = ITEM_NON_EXISTING_ID;
  }

  // Setup time measurement.
  auto last_ping = std::chrono::steady_clock::now();

  while (!ctx->end) {
    state.now = std::chrono::steady_clock::now();
    bool processed_any_events =
        this->process_ui_event() ||
        this->process_net_event();

    if (!processed_any_events) {
      // Perhaps send a ping?
      std::chrono::duration<float> diff = state.now - last_ping;
      if (diff.count() > 30.0f) {  // Seconds.
        last_ping = state.now;
        ctx->queue_net_to->push(EventGameNet{
            EventGameNet::PACKET,
            PacketsCS_PING::make().release()});
      }

      // Good night.
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
}
