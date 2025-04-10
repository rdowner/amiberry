/*
 * UAE - The Un*x Amiga Emulator
 *
 * Amiberry interface
 *
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include "sysdeps.h"
#include "uae.h"
#include "options.h"
#include "custom.h"
#include "rommgr.h"
#include "fsdb.h"
#include "tinyxml2.h"
#include <fstream>

extern void SetLastActiveConfig(const char* filename);
extern char current_dir[MAX_DPATH];
extern char last_loaded_config[MAX_DPATH];

// Use configs with 8MB Fast RAM, to make it likely
// that WHDLoad preload will cache everything.
#define A600_CONFIG  3 // 8MB fast ram
#define A1200_CONFIG 2 // 8MB fast ram

struct game_options
{
	std::string port0 = "nul";
	std::string port1 = "nul";
	std::string control = "nul";
	std::string control2 = "nul";
	std::string cpu = "nul";
	std::string blitter = "nul";
	std::string clock = "nul";
	std::string chipset = "nul";
	std::string jit = "nul";
	std::string cpu_comp = "nul";
	std::string cpu_24bit = "nul";
	std::string cpu_exact = "nul";
	std::string sprites = "nul";
	std::string scr_width = "nul";
	std::string scr_height = "nul";
	std::string scr_autoheight = "nul";
	std::string scr_centerh = "nul";
	std::string scr_centerv = "nul";
	std::string scr_offseth = "nul";
	std::string scr_offsetv = "nul";
	std::string ntsc = "nul";
	std::string chip = "nul";
	std::string fast = "nul";
	std::string z3 = "nul";
	std::string fastcopper = "nul";
};

TCHAR whdboot_path[MAX_DPATH];
TCHAR boot_path[MAX_DPATH];
TCHAR save_path[MAX_DPATH];
TCHAR config_path[MAX_DPATH];
TCHAR whd_path[MAX_DPATH];
TCHAR kick_path[MAX_DPATH];

TCHAR uae_config[255];
TCHAR whd_config[255];
TCHAR whd_startup[255];

TCHAR game_name[MAX_DPATH];
TCHAR selected_slave[MAX_DPATH];
TCHAR sub_path[MAX_DPATH];
TCHAR use_slave_libs = false;

static TCHAR* parse_text(const TCHAR* s)
{
	if (*s == '"' || *s == '\'')
	{
		const auto c = *s++;
		auto* const d = my_strdup(s);
		for (unsigned int i = 0; i < _tcslen(d); i++)
		{
			if (d[i] == c)
			{
				d[i] = 0;
				break;
			}
		}
		return d;
	}
	return my_strdup(s);
}

std::string trim_full_line(std::string full_line)
{
	const std::string tab = "\t";

	auto found = full_line.find(tab);
	while (found != std::string::npos)
	{
		full_line.replace(found, tab.size(), "");
		found = full_line.find(tab);
	}

	return full_line;
}

std::string find_substring(const std::string& search_string, const std::string& whole_string)
{
	const std::string lf = "\n";
	const auto check = string(search_string);
	auto full_line = trim_full_line(string(whole_string));

	auto lf_found = full_line.find(lf);
	while (lf_found != std::string::npos)
	{
		const auto start = full_line.find_first_of(lf, lf_found);
		auto end = full_line.find_first_of(lf, start + 1);
		if (end == std::string::npos) end = full_line.size();

		const auto found = full_line.find(check);
		if (found != std::string::npos)
		{
			const auto found_end = full_line.find_first_of(lf, found);
			auto result = full_line.substr(found + check.size() + 1, found_end - found - check.size() - 1);
			return result;
		}

		if (end < full_line.size())
			lf_found = end;
		else
			lf_found = std::string::npos;
	}

	return "nul";
}

void parse_cfg_line(uae_prefs* prefs, const std::string& line_string)
{
	TCHAR* line = my_strdup(line_string.c_str());
	cfgfile_parse_line(prefs, line, 0);
	xfree(line);
}

void parse_custom_settings(uae_prefs* p, const char* settings)
{
	const std::string lf = "\n";
	const std::string check = "amiberry_custom";
	auto full_line = trim_full_line(string(settings));

	auto lf_found = full_line.find(lf);
	while (lf_found != std::string::npos)
	{
		const auto start = full_line.find_first_of(lf, lf_found);
		auto end = full_line.find_first_of(lf, start + 1);
		if (end == std::string::npos) end = full_line.size();

		std::string line = full_line.substr(start + 1, end - start - 1);
		if (line.find(check) != std::string::npos)
		{
			parse_cfg_line(p, line);
		}

		if (end < full_line.size())
			lf_found = end;
		else
			lf_found = std::string::npos;
	}
}

std::string find_whdload_game_option(const TCHAR* find_setting, const char* whd_options)
{
	return find_substring(string(find_setting), string(whd_options));
}

game_options get_game_settings(const char* HW)
{
	game_options output_detail;
	output_detail.port0 = find_whdload_game_option("PORT0", HW);
	output_detail.port1 = find_whdload_game_option("PORT1", HW);
	output_detail.control = find_whdload_game_option("PRIMARY_CONTROL", HW);
	output_detail.control2 = find_whdload_game_option("SECONDARY_CONTROL", HW);
	output_detail.cpu = find_whdload_game_option("CPU", HW);
	output_detail.blitter = find_whdload_game_option("BLITTER", HW);
	output_detail.clock = find_whdload_game_option("CLOCK", HW);
	output_detail.chipset = find_whdload_game_option("CHIPSET", HW);
	output_detail.jit = find_whdload_game_option("JIT", HW);
	output_detail.cpu_24bit = find_whdload_game_option("CPU_24BITADDRESSING", HW);
	output_detail.cpu_comp = find_whdload_game_option("CPU_COMPATIBLE", HW);
	output_detail.sprites = find_whdload_game_option("SPRITES", HW);
	output_detail.scr_height = find_whdload_game_option("SCREEN_HEIGHT", HW);
	output_detail.scr_width = find_whdload_game_option("SCREEN_WIDTH", HW);
	output_detail.scr_autoheight = find_whdload_game_option("SCREEN_AUTOHEIGHT", HW);
	output_detail.scr_centerh = find_whdload_game_option("SCREEN_CENTERH", HW);
	output_detail.scr_centerv = find_whdload_game_option("SCREEN_CENTERV", HW);
	output_detail.scr_offseth = find_whdload_game_option("SCREEN_OFFSETH", HW);
	output_detail.scr_offsetv = find_whdload_game_option("SCREEN_OFFSETV", HW);
	output_detail.ntsc = find_whdload_game_option("NTSC", HW);
	output_detail.fast = find_whdload_game_option("FAST_RAM", HW);
	output_detail.z3 = find_whdload_game_option("Z3_RAM", HW);
	output_detail.cpu_exact = find_whdload_game_option("CPU_EXACT", HW);
	output_detail.fastcopper = find_whdload_game_option("FAST_COPPER", HW);

	return output_detail;
}

void make_rom_symlink(const char* kick_short, int kick_numb, struct uae_prefs* p)
{
	char kick_long[MAX_DPATH];
	int roms[2] = { -1,-1 };

	// do the checks...
	_sntprintf(kick_long, MAX_DPATH, "%s/%s", kick_path, kick_short);

	// this should sort any broken links (only remove if a link, not a file. See vfat handling of link below)
	// Only remove file IF it is a symlink.
	// On VFAT USB stick, the roms are copied not symlinked, which takes a long time, also the user
	// may have provided their own rom files, so we do not want to remove and replace those.
	//
	if (my_existslink(kick_long)) {
		my_unlink(kick_long);
	}

	if (!my_existsfile2(kick_long))
	{
		roms[0] = kick_numb;
		const auto rom_test = configure_rom(p, roms, 0);
		if (rom_test == 1)
		{
			int r = symlink(p->romfile, kick_long);
			// VFAT filesystems do not support creation of symlinks.
			// Fallback to copying file if filesystem does not support the generation of symlinks
			if (r < 0 && errno == EPERM)
				r = copyfile(kick_long, p->romfile, true);
			write_log("Making SymLink for Kickstart ROM: %s  [%s]\n", kick_long, r < 0 ? "Fail" : "Ok");
		}
	}
}

void symlink_roms(struct uae_prefs* prefs)
{
	TCHAR tmp[MAX_DPATH];
	TCHAR tmp2[MAX_DPATH];

	write_log("SymLink Kickstart ROMs for Booter\n");

	// here we can do some checks for Kickstarts we might need to make symlinks for
	_tcsncpy(current_dir, start_path_data, MAX_DPATH);

	// are we using save-data/ ?
	get_savedatapath(tmp, MAX_DPATH, 1);
	_sntprintf(kick_path, MAX_DPATH, _T("%s/Kickstarts"), tmp);

	if (!my_existsdir(kick_path)) {
		// otherwise, use the old route
		get_whdbootpath(whdboot_path, MAX_DPATH);
		_sntprintf(kick_path, MAX_DPATH, _T("%sgame-data/Devs/Kickstarts"), whdboot_path);
	}
	write_log("WHDBoot - using kickstarts from %s\n", kick_path);

	// These are all the kickstart rom files found in skick346.lha
	//   http://aminet.net/package/util/boot/skick346

	make_rom_symlink("kick33180.A500", 5, prefs);
	make_rom_symlink("kick34005.A500", 6, prefs);
	make_rom_symlink("kick37175.A500", 7, prefs);
	make_rom_symlink("kick39106.A1200", 11, prefs);
	make_rom_symlink("kick40063.A600", 14, prefs);
	make_rom_symlink("kick40068.A1200", 15, prefs);
	make_rom_symlink("kick40068.A4000", 16, prefs);

	// Symlink rom.key also
	// source file
	get_rom_path(tmp2, MAX_DPATH);
	_sntprintf(tmp, MAX_DPATH, _T("%s/rom.key"), tmp2);

	// destination file (symlink)
	_sntprintf(tmp2, MAX_DPATH, _T("%s/rom.key"), kick_path);

	if (my_existsfile2(tmp)) {
		const int r = symlink(tmp, tmp2);
		if (r < 0 && errno == EPERM)
			copyfile(tmp2, tmp, true);
	}
}

void get_game_name(char* filepath)
{
	extract_filename(filepath, last_loaded_config);
	extract_filename(filepath, game_name);
	remove_file_extension(game_name);
}

void set_jport_modes(uae_prefs* prefs, const bool is_cd32)
{
	if (is_cd32)
	{
		prefs->jports[0].mode = 7;
		prefs->jports[1].mode = 7;
	}
	else
	{
		// JOY
		prefs->jports[1].mode = 3;
		// MOUSE
		prefs->jports[0].mode = 2;
	}
}
void clear_jports(uae_prefs* prefs)
{
	for (auto& jport : prefs->jports)
	{
		jport.id = JPORT_NONE;
		jport.idc.configname[0] = 0;
		jport.idc.name[0] = 0;
		jport.idc.shortid[0] = 0;
	}
}

void build_uae_config_filename()
{
	_tcscpy(uae_config, config_path);
	_tcscat(uae_config, game_name);
	_tcscat(uae_config, ".uae");
}

void cd_auto_prefs(uae_prefs* prefs, char* filepath)
{
	TCHAR tmp[MAX_DPATH];

	write_log("\nCD Autoload: %s  \n\n", filepath);

	get_configuration_path(config_path, MAX_DPATH);
	get_game_name(filepath);

	// LOAD GAME SPECIFICS FOR EXISTING .UAE - USE SHA1 IF AVAILABLE
	//  CONFIG LOAD IF .UAE IS IN CONFIG PATH
	build_uae_config_filename();

	if (my_existsfile2(uae_config))
	{
		target_cfgfile_load(prefs, uae_config, CONFIG_TYPE_ALL, 0);
		return;
	}

	prefs->start_gui = false;

	const auto is_cdtv = _tcsstr(filepath, _T("CDTV")) != nullptr || _tcsstr(filepath, _T("cdtv")) != nullptr;
	const auto is_cd32 = _tcsstr(filepath, _T("CD32")) != nullptr || _tcsstr(filepath, _T("cd32")) != nullptr;

	// CD32
	if (is_cd32)
	{
		_tcscpy(prefs->description, _T("AutoBoot Configuration [CD32]"));
		// SET THE BASE AMIGA (CD32)
		built_in_prefs(prefs, 8, 0, 0, 0);
	}
	else if (is_cdtv)
	{
		_tcscpy(prefs->description, _T("AutoBoot Configuration [CDTV]"));
		// SET THE BASE AMIGA (CDTV)
		built_in_prefs(prefs, 9, 0, 0, 0);
	}
	else
	{
		_tcscpy(prefs->description, _T("AutoBoot Configuration [A1200CD]"));
		// SET THE BASE AMIGA (Expanded A1200)
		built_in_prefs(prefs, 4, A1200_CONFIG, 0, 0);
	}

	// enable CD
	_stprintf(tmp, "cd32cd=1");
	cfgfile_parse_line(prefs, parse_text(tmp), 0);

	// mount the image
	_stprintf(tmp, "cdimage0=%s,image", filepath);
	cfgfile_parse_line(prefs, parse_text(tmp), 0);

	//APPLY THE SETTINGS FOR MOUSE/JOYSTICK ETC
	set_jport_modes(prefs, is_cd32);

	// APPLY SPECIAL CONFIG E.G. MOUSE OR ALT. JOYSTICK SETTINGS
	clear_jports(prefs);

	// WHAT IS THE MAIN CONTROL?
	// PORT 0 - MOUSE
	std::string line_string;
	if (is_cd32 && strcmpi(amiberry_options.default_controller2, "") != 0)
	{
		line_string = "joyport0=";
		line_string.append(amiberry_options.default_controller2);
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(amiberry_options.default_mouse1, "") != 0)
	{
		line_string = "joyport0=";
		line_string.append(amiberry_options.default_mouse1);
		parse_cfg_line(prefs, line_string);
	}
	else
	{
		line_string = "joyport0=mouse";
		parse_cfg_line(prefs, line_string);
	}

	// PORT 1 - JOYSTICK
	if (strcmpi(amiberry_options.default_controller1, "") != 0)
	{
		line_string = "joyport1=";
		line_string.append(amiberry_options.default_controller1);
		parse_cfg_line(prefs, line_string);
	}
	else
	{
		line_string = "joyport1=joy1";
		parse_cfg_line(prefs, line_string);
	}
}

void set_input_settings(uae_prefs* prefs, const game_options& game_detail, const bool is_cd32)
{
	// APPLY SPECIAL CONFIG E.G. MOUSE OR ALT. JOYSTICK SETTINGS
	clear_jports(prefs);

	//  CD32
	if (is_cd32 || strcmpi(game_detail.port0.c_str(), "cd32") == 0)
		prefs->jports[0].mode = 7;

	if (is_cd32	|| strcmpi(game_detail.port1.c_str(), "cd32") == 0)
		prefs->jports[1].mode = 7;

	// JOY
	if (strcmpi(game_detail.port0.c_str(), "joy") == 0)
		prefs->jports[0].mode = 0;
	if (strcmpi(game_detail.port1.c_str(), "joy") == 0)
		prefs->jports[1].mode = 0;

	// MOUSE
	if (strcmpi(game_detail.port0.c_str(), "mouse") == 0)
		prefs->jports[0].mode = 2;
	if (strcmpi(game_detail.port1.c_str(), "mouse") == 0)
		prefs->jports[1].mode = 2;

	// WHAT IS THE MAIN CONTROL?
	// PORT 0 - MOUSE GAMES
	std::string line_string;
	if (strcmpi(game_detail.control.c_str(), "mouse") == 0 && strcmpi(amiberry_options.default_mouse1, "") != 0)
	{
		line_string = "joyport0=";
		line_string.append(amiberry_options.default_mouse1);
		parse_cfg_line(prefs, line_string);
	}
	// PORT 0 - JOYSTICK GAMES
	else if (strcmpi(amiberry_options.default_controller2, "") != 0)
	{
		line_string = "joyport0=";
		line_string.append(amiberry_options.default_controller2);
		parse_cfg_line(prefs, line_string);
	}
	else
	{
		line_string = "joyport0=mouse";
		parse_cfg_line(prefs, line_string);
	}

	// PORT 1 - MOUSE GAMES
	if (strcmpi(game_detail.control.c_str(), "mouse") == 0 && strcmpi(amiberry_options.default_mouse2, "") != 0)
	{
		line_string = "joyport1=";
		line_string.append(amiberry_options.default_mouse2);
		parse_cfg_line(prefs, line_string);
	}
	// PORT 1 - JOYSTICK GAMES
	else if (strcmpi(amiberry_options.default_controller1, "") != 0)
	{
		line_string = "joyport1=";
		line_string.append(amiberry_options.default_controller1);
		parse_cfg_line(prefs, line_string);
	}
	else
	{
		line_string = "joyport1=joy1";
		parse_cfg_line(prefs, line_string);
	}

	// PARALLEL PORT GAMES
	if (strcmpi(amiberry_options.default_controller3, "") != 0)
	{
		line_string = "joyport2=";
		line_string.append(amiberry_options.default_controller3);
		parse_cfg_line(prefs, line_string);
	}
	if (strcmpi(amiberry_options.default_controller4, "") != 0)
	{
		line_string = "joyport3=";
		line_string.append(amiberry_options.default_controller4);
		parse_cfg_line(prefs, line_string);
	}
}

void parse_gfx_settings(uae_prefs* prefs, const game_options& game_detail)
{
	std::string line_string;
	// SCREEN AUTO-HEIGHT
	if (strcmpi(game_detail.scr_autoheight.c_str(), "true") == 0)
	{
		line_string = "amiberry.gfx_auto_crop=true";
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(game_detail.scr_autoheight.c_str(), "false") == 0)
	{
		line_string = "amiberry.gfx_auto_crop=false";
		parse_cfg_line(prefs, line_string);
	}

	// SCREEN CENTER/HEIGHT/WIDTH
	if (strcmpi(game_detail.scr_centerh.c_str(), "smart") == 0)
	{
#ifdef USE_DISPMANX
		line_string = "gfx_center_horizontal=smart";
		parse_cfg_line(prefs, line_string);
#else
		if (prefs->gfx_auto_crop)
		{
			// Disable if using Auto-Crop, otherwise the output won't be correct
			line_string = "gfx_center_horizontal=none";
			parse_cfg_line(prefs, line_string);
		}
		else
		{
			line_string = "gfx_center_horizontal=smart";
			parse_cfg_line(prefs, line_string);
		}
#endif
	}
	else if (strcmpi(game_detail.scr_centerh.c_str(), "none") == 0)
	{
		line_string = "gfx_center_horizontal=none";
		parse_cfg_line(prefs, line_string);
	}

	if (strcmpi(game_detail.scr_centerv.c_str(), "smart") == 0)
	{
#ifdef USE_DISPMANX
		line_string = "gfx_center_vertical=smart";
		parse_cfg_line(prefs, line_string);
#else
		if (prefs->gfx_auto_crop)
		{
			// Disable if using Auto-Crop, otherwise the output won't be correct
			line_string = "gfx_center_vertical=none";
			parse_cfg_line(prefs, line_string);
		}
		else
		{
			line_string = "gfx_center_vertical=smart";
			parse_cfg_line(prefs, line_string);
		}
#endif
	}
	else if (strcmpi(game_detail.scr_centerv.c_str(), "none") == 0)
	{
		line_string = "gfx_center_vertical=none";
		parse_cfg_line(prefs, line_string);
	}

	if (strcmpi(game_detail.scr_height.c_str(), "nul") != 0)
	{
#ifdef USE_DISPMANX
		line_string = "gfx_height=";
		line_string.append(game_detail.scr_height);
		parse_cfg_line(prefs, line_string);

		line_string = "gfx_height_windowed=";
		line_string.append(game_detail.scr_height);
		parse_cfg_line(prefs, line_string);

		line_string = "gfx_height_fullscreen=";
		line_string.append(game_detail.scr_height);
		parse_cfg_line(prefs, line_string);
#else
		if (prefs->gfx_auto_crop)
		{
			// If using Auto-Crop, bypass any screen Height adjustments as they are not needed and will cause issues
			line_string = "gfx_height=768";
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_height_windowed=768";
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_height_fullscreen=768";
			parse_cfg_line(prefs, line_string);
		}
		else
		{
			line_string = "gfx_height=";
			line_string.append(game_detail.scr_height);
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_height_windowed=";
			line_string.append(game_detail.scr_height);
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_height_fullscreen=";
			line_string.append(game_detail.scr_height);
			parse_cfg_line(prefs, line_string);
		}
#endif
	}

	if (strcmpi(game_detail.scr_width.c_str(), "nul") != 0)
	{
#ifdef USE_DISPMANX
		line_string = "gfx_width=";
		line_string.append(game_detail.scr_width);
		parse_cfg_line(prefs, line_string);

		line_string = "gfx_width_windowed=";
		line_string.append(game_detail.scr_width);
		parse_cfg_line(prefs, line_string);

		line_string = "gfx_width_fullscreen=";
		line_string.append(game_detail.scr_width);
		parse_cfg_line(prefs, line_string);
#else
		if (prefs->gfx_auto_crop)
		{
			// If using Auto-Crop, bypass any screen Width adjustments as they are not needed and will cause issues
			line_string = "gfx_width=720";
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_width_windowed=720";
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_width_fullscreen=720";
			parse_cfg_line(prefs, line_string);
		}
		else
		{
			line_string = "gfx_width=";
			line_string.append(game_detail.scr_width);
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_width_windowed=";
			line_string.append(game_detail.scr_width);
			parse_cfg_line(prefs, line_string);

			line_string = "gfx_width_fullscreen=";
			line_string.append(game_detail.scr_width);
			parse_cfg_line(prefs, line_string);
		}
#endif
	}

	if (strcmpi(game_detail.scr_offseth.c_str(), "nul") != 0)
	{
#ifdef USE_DISPMANX
		line_string = "amiberry.gfx_horizontal_offset=";
		line_string.append(game_detail.scr_offseth);
		parse_cfg_line(prefs, line_string);
#else
		if (prefs->gfx_auto_crop)
		{
			line_string = "amiberry.gfx_horizontal_offset=0";
			parse_cfg_line(prefs, line_string);
		}
		else
		{
			line_string = "amiberry.gfx_horizontal_offset=";
			line_string.append(game_detail.scr_offseth);
			parse_cfg_line(prefs, line_string);
		}
#endif
	}

	if (strcmpi(game_detail.scr_offsetv.c_str(), "nul") != 0)
	{
#ifdef USE_DISPMANX
		line_string = "amiberry.gfx_vertical_offset=";
		line_string.append(game_detail.scr_offsetv);
		parse_cfg_line(prefs, line_string);
#else
		if (prefs->gfx_auto_crop)
		{
			line_string = "amiberry.gfx_vertical_offset=0";
			parse_cfg_line(prefs, line_string);
		}
		else
		{
			line_string = "amiberry.gfx_vertical_offset=";
			line_string.append(game_detail.scr_offsetv);
			parse_cfg_line(prefs, line_string);
		}
#endif
	}
}

void set_compatibility_settings(uae_prefs* prefs, const game_options& game_detail, const bool a600_available, const bool use_aga)
{
	std::string line_string;
	// CPU 68020/040 or no A600 ROM available
	if (strcmpi(game_detail.cpu.c_str(), "68020") == 0 || strcmpi(game_detail.cpu.c_str(), "68040") == 0 || use_aga)
	{
		line_string = "cpu_type=";
		line_string.append(use_aga ? "68020" : game_detail.cpu);
		parse_cfg_line(prefs, line_string);
	}

	// CPU 68000/010 [requires a600 rom)]
	else if ((strcmpi(game_detail.cpu.c_str(), "68000") == 0 || strcmpi(game_detail.cpu.c_str(), "68010") == 0) && a600_available)
	{
		line_string = "cpu_type=";
		line_string.append(game_detail.cpu);
		parse_cfg_line(prefs, line_string);

		line_string = "chipmem_size=4";
		parse_cfg_line(prefs, line_string);
	}

	// Invalid or no CPU value specified, but A600 ROM is available? Use 68000
	else if (a600_available)
	{
		write_log("Invalid or no CPU value, A600 ROM available, using CPU: 68000\n");
		line_string = "cpu_type=68000";
		parse_cfg_line(prefs, line_string);
	}
	// Fallback for any invalid values - 68020 CPU
	else
	{
		write_log("Invalid or no CPU value, A600 ROM NOT found, falling back to CPU: 68020\n");
		line_string = "cpu_type=68020";
		parse_cfg_line(prefs, line_string);
	}

	// CPU SPEED
	if (strcmpi(game_detail.clock.c_str(), "7") == 0)
	{
		line_string = "cpu_speed=real";
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(game_detail.clock.c_str(), "14") == 0)
	{
		line_string = "finegrain_cpu_speed=1024";
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(game_detail.clock.c_str(), "28") == 0 || strcmpi(game_detail.clock.c_str(), "25") == 0)
	{
		line_string = "finegrain_cpu_speed=128";
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(game_detail.clock.c_str(), "max") == 0)
	{
		line_string = "cpu_speed=max";
		parse_cfg_line(prefs, line_string);
	}

	// COMPATIBLE CPU
	if (strcmpi(game_detail.cpu_comp.c_str(), "true") == 0)
	{
		line_string = "cpu_compatible=true";
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(game_detail.cpu_comp.c_str(), "false") == 0)
	{
		line_string = "cpu_compatible=false";
		parse_cfg_line(prefs, line_string);
	}

	// COMPATIBLE CPU
	if (strcmpi(game_detail.cpu_24bit.c_str(), "false") == 0 || strcmpi(game_detail.z3.c_str(), "nul") != 0)
	{
		line_string = "cpu_24bit_addressing=false";
		parse_cfg_line(prefs, line_string);
	}

	// CYCLE-EXACT CPU
	if (strcmpi(game_detail.cpu_exact.c_str(), "true") == 0)
	{
		line_string = "cpu_cycle_exact=true";
		parse_cfg_line(prefs, line_string);
	}

	// FAST / Z3 MEMORY REQUIREMENTS
	if (strcmpi(game_detail.fast.c_str(), "nul") != 0)
	{
		line_string = "fastmem_size=";
		line_string.append(game_detail.fast);
		parse_cfg_line(prefs, line_string);
	}
	if (strcmpi(game_detail.z3.c_str(), "nul") != 0)
	{
		line_string = "z3mem_size=";
		line_string.append(game_detail.z3);
		parse_cfg_line(prefs, line_string);
	}

	// FAST COPPER
	if (strcmpi(game_detail.fastcopper.c_str(), "true") == 0)
	{
		line_string = "fast_copper=true";
		parse_cfg_line(prefs, line_string);
	}

	// BLITTER=IMMEDIATE/WAIT/NORMAL
	if (strcmpi(game_detail.blitter.c_str(), "immediate") == 0)
	{
		line_string = "immediate_blits=true";
		parse_cfg_line(prefs, line_string);
	}
	else if (strcmpi(game_detail.blitter.c_str(), "normal") == 0)
	{
		line_string = "waiting_blits=disabled";
		parse_cfg_line(prefs, line_string);
	}

	// JIT
	if (strcmpi(game_detail.jit.c_str(), "true") == 0)
	{
		line_string = "cachesize=16384";
		parse_cfg_line(prefs, line_string);

		line_string = "cpu_compatible=false";
		parse_cfg_line(prefs, line_string);

		line_string = "cpu_cycle_exact=false";
		parse_cfg_line(prefs, line_string);

		line_string = "cpu_memory_cycle_exact=false";
		parse_cfg_line(prefs, line_string);

		line_string = "address_space_24=false";
		parse_cfg_line(prefs, line_string);
	}

	// NTSC
	if (strcmpi(game_detail.ntsc.c_str(), "true") == 0)
	{
		line_string = "ntsc=true";
		parse_cfg_line(prefs, line_string);
	}

	// SPRITE COLLISION
	if (strcmpi(game_detail.sprites.c_str(), "nul") != 0)
	{
		line_string = "collision_level=";
		line_string.append(game_detail.sprites);
		parse_cfg_line(prefs, line_string);
	}

	// Screen settings, only if allowed to override the defaults from amiberry.conf
	if (amiberry_options.allow_display_settings_from_xml)
	{
		parse_gfx_settings(prefs, game_detail);
	}
}

game_options parse_settings_from_xml(uae_prefs* prefs, char* filepath)
{
	game_options game_detail;
	tinyxml2::XMLDocument doc;
	auto error = false;
	write_log("WHDBooter - Loading whdload_db.xml\n");
	write_log("WHDBooter - Searching whdload_db.xml for %s\n", game_name);

	auto* f = fopen(whd_config, _T("rb"));
	if (f)
	{
		auto err = doc.LoadFile(f);
		if (err != tinyxml2::XML_SUCCESS)
		{
			write_log(_T("Failed to parse '%s':  %d\n"), whd_config, err);
			error = true;
		}
		fclose(f);
	}
	else
	{
		error = true;
	}

	if (!error)
	{
		auto sha1 = my_get_sha1_of_file(filepath);
		std::transform(sha1.begin(), sha1.end(), sha1.begin(), ::tolower);
		auto* game_node = doc.FirstChildElement("whdbooter");
		game_node = game_node->FirstChildElement("game");
		while (game_node != nullptr)
		{
			// Ideally we'd just match by sha1, but filename has worked up until now, so try that first
			// then fall back to sha1 if a user has renamed the file!
			//
			int found = 0;
			if (game_node->Attribute("filename", game_name))
			{
				found = 1;
			}
			if (game_node->Attribute("sha1", sha1.c_str()))
			{
				found = 1;
			}

			if (found)
			{
				// now get the <hardware> and <custom_controls> items
				// get hardware
				auto* temp_node = game_node->FirstChildElement("hardware");
				if (temp_node)
				{
					const auto* hardware = temp_node->GetText();
					if (hardware)
					{
						game_detail = get_game_settings(hardware);
						write_log("WHDBooter - Game H/W Settings: \n%s\n", hardware);
					}
				}
					
				// get custom controls
				temp_node = game_node->FirstChildElement("custom_controls");
				if (temp_node)
				{
					const auto* custom_settings = temp_node->GetText();
					if (custom_settings)
					{
						write_log("WHDBooter - Game Custom Settings: \n%s\n", custom_settings);
						parse_custom_settings(prefs, custom_settings);
					}
				}

				if (strlen(selected_slave) == 0)
				{
					temp_node = game_node->FirstChildElement("slave_default");

					// use a selected slave if we have one
					if (strlen(prefs->whdbootprefs.slave) != 0)
					{
						_tcscpy(selected_slave, prefs->whdbootprefs.slave);
						write_log("WHDBooter - Config Selected Slave: %s \n", selected_slave);
					}
					// otherwise use the XML default
					else if (temp_node->GetText() != nullptr)
					{
						_stprintf(selected_slave, "%s", temp_node->GetText());
						write_log("WHDBooter - Default Slave: %s\n", selected_slave);
					}

					temp_node = game_node->FirstChildElement("subpath");

					if (temp_node->GetText() != nullptr)
					{
						_stprintf(sub_path, "%s", temp_node->GetText());
						write_log("WHDBooter - SubPath:  %s\n", sub_path);
					}
				}

				// get slave_libraries
				temp_node = game_node->FirstChildElement("slave_libraries");
				if (temp_node->GetText() != nullptr)
				{
					if (strcmpi(temp_node->GetText(), "true") == 0)
						use_slave_libs = true;

					write_log("WHDBooter - Libraries:  %s\n", sub_path);
				}
				break;
			}
			game_node = game_node->NextSiblingElement();
		}
	}
	return game_detail;
}

void create_startup_sequence(uae_prefs* prefs)
{
	std::ostringstream whd_bootscript;
	whd_bootscript << "FAILAT 999\n";

	if (use_slave_libs)
	{
		whd_bootscript << "DH3:C/Assign LIBS: DH3:LIBS/ ADD\n";
	}

	whd_bootscript << "IF NOT EXISTS WHDLoad\n";
	whd_bootscript << "DH3:C/Assign C: DH3:C/ ADD\n";
	whd_bootscript << "ENDIF\n";

	whd_bootscript << "CD \"Games:" << sub_path << "\"\n";
	whd_bootscript << "WHDLoad SLAVE=\"Games:" << sub_path << "/" << selected_slave << "\"";

	// Write Cache
	if (prefs->whdbootprefs.writecache)
	{
		whd_bootscript << " PRELOAD NOREQ ";
	}
	else
	{
		whd_bootscript << " PRELOAD NOREQ NOWRITECACHE ";
	}

	// CUSTOM options
	if (prefs->whdbootprefs.custom1 > 0)
	{
		whd_bootscript << " CUSTOM1=" << prefs->whdbootprefs.custom1;
	}
	if (prefs->whdbootprefs.custom2 > 0)
	{
		whd_bootscript << " CUSTOM2=" << prefs->whdbootprefs.custom2;
	}
	if (prefs->whdbootprefs.custom3 > 0)
	{
		whd_bootscript << " CUSTOM3=" << prefs->whdbootprefs.custom3;
	}
	if (prefs->whdbootprefs.custom4 > 0)
	{
		whd_bootscript << " CUSTOM4=" << prefs->whdbootprefs.custom4;
	}
	if (prefs->whdbootprefs.custom5 > 0)
	{
		whd_bootscript << " CUSTOM5=" << prefs->whdbootprefs.custom5;
	}
	if (strlen(prefs->whdbootprefs.custom) != 0)
	{
		whd_bootscript << " CUSTOM=\"" << prefs->whdbootprefs.custom << "\"";
	}

	// BUTTONWAIT
	if (prefs->whdbootprefs.buttonwait)
	{
		whd_bootscript << " BUTTONWAIT";
	}

	// SPLASH
	if (!prefs->whdbootprefs.showsplash)
	{
		whd_bootscript << " SPLASHDELAY=0";
	}

	// CONFIGDELAY
	if (prefs->whdbootprefs.configdelay != 0)
	{
		whd_bootscript << " CONFIGDELAY=" << prefs->whdbootprefs.configdelay;
	}

	// SPECIAL SAVE PATH
	whd_bootscript << " SAVEPATH=Saves:Savegames/ SAVEDIR=\"" << sub_path << "\"";
	whd_bootscript << '\n';

	// Launches utility program to quit the emulator (via a UAE trap in RTAREA)
	if (prefs->whdbootprefs.quit_on_exit)
	{
		whd_bootscript << "DH0:C/AmiQuit\n";
	}

	write_log("WHDBooter - Created Startup-Sequence  \n\n%s\n", whd_bootscript.str().c_str());
	write_log("WHDBooter - Saved Auto-Startup to %s\n", whd_startup);

	ofstream myfile(whd_startup);
	if (myfile.is_open())
	{
		myfile << whd_bootscript.str();
		myfile.close();
	}
}

bool is_a600_available(uae_prefs* prefs)
{
	int roms[3] = { 10, 9, -1 }; // kickstart 2.05 A600HD
	const auto rom_test = configure_rom(prefs, roms, 0);
	return rom_test == 1;
}

void set_booter_drives(uae_prefs* prefs, char* filepath)
{
	TCHAR tmp[MAX_DPATH];

	if (strlen(selected_slave) != 0) // new booter solution
	{
		_sntprintf(boot_path, MAX_DPATH, "/tmp/amiberry/");

		_stprintf(tmp, _T("filesystem2=rw,DH0:DH0:%s,10"), boot_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);

		_stprintf(tmp, _T("uaehf0=dir,rw,DH0:DH0::%s,10"), boot_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);

		_sntprintf(boot_path, MAX_DPATH, "%sboot-data.zip", whdboot_path);
		if (!my_existsfile2(boot_path))
			_sntprintf(boot_path, MAX_DPATH, "%sboot-data/", whdboot_path);

		_stprintf(tmp, _T("filesystem2=rw,DH3:DH3:%s,-10"), boot_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);

		_stprintf(tmp, _T("uaehf0=dir,rw,DH3:DH3::%s,-10"), boot_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);
	}
	else // revert to original booter is no slave was set
	{
		_sntprintf(boot_path, MAX_DPATH, "%sboot-data.zip", whdboot_path);
		if (!my_existsfile2(boot_path))
			_sntprintf(boot_path, MAX_DPATH, "%sboot-data/", whdboot_path);

		_stprintf(tmp, _T("filesystem2=rw,DH0:DH0:%s,10"), boot_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);

		_stprintf(tmp, _T("uaehf0=dir,rw,DH0:DH0::%s,10"), boot_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);
	}

	//set the Second (game data) drive
	_stprintf(tmp, "filesystem2=rw,DH1:Games:\"%s\",0", filepath);
	cfgfile_parse_line(prefs, parse_text(tmp), 0);

	_stprintf(tmp, "uaehf1=dir,rw,DH1:Games:\"%s\",0", filepath);
	cfgfile_parse_line(prefs, parse_text(tmp), 0);

	//set the third (save data) drive
	_sntprintf(whd_path, MAX_DPATH, "%s/", save_path);

	if (my_existsdir(save_path))
	{
		_stprintf(tmp, "filesystem2=rw,DH2:Saves:%s,0", save_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);

		_stprintf(tmp, "uaehf2=dir,rw,DH2:Saves:%s,0", save_path);
		cfgfile_parse_line(prefs, parse_text(tmp), 0);
	}
}

void whdload_auto_prefs(uae_prefs* prefs, char* filepath)
{
	write_log("WHDBooter Launched\n");
	_tcscpy(selected_slave, "");

	get_configuration_path(config_path, MAX_DPATH);
	get_whdbootpath(whdboot_path, MAX_DPATH);
	get_savedatapath(save_path, MAX_DPATH, 0 );

	symlink_roms(prefs);

	// this allows A600HD to be used to slow games down
	const auto a600_available = is_a600_available(prefs);
	if (a600_available)
	{
		write_log("WHDBooter - Host: A600 ROM available, will use it for non-AGA titles\n");
	}
	else
	{
		write_log("WHDBooter - Host: A600 ROM not found, falling back to A1200 config for all titles\n");
	}

	// REMOVE THE FILE PATH AND EXTENSION
	const auto* filename = my_getfilepart(filepath);
	get_game_name(filepath);

	// LOAD GAME SPECIFICS FOR EXISTING .UAE - USE SHA1 IF AVAILABLE
	//  CONFIG LOAD IF .UAE IS IN CONFIG PATH
	build_uae_config_filename();

	// If we have a config file, we will use it.
	// We will need it for the WHDLoad options too.
	if (my_existsfile2(uae_config))
	{
		write_log("WHDBooter -  %s found. Loading Config for WHDLoad options.\n", uae_config);
		target_cfgfile_load(&currprefs, uae_config, CONFIG_TYPE_ALL, 0);
	}

	// setups for tmp folder.
	my_mkdir("/tmp/amiberry");
	my_mkdir("/tmp/amiberry/s");
	my_mkdir("/tmp/amiberry/c");
	my_mkdir("/tmp/amiberry/devs");
	_tcscpy(whd_startup, "/tmp/amiberry/s/startup-sequence");
	remove(whd_startup);

	// LOAD HOST OPTIONS
	_sntprintf(whd_path, MAX_DPATH, "%sWHDLoad", whdboot_path);

	// are we using save-data/ ?
	_sntprintf(kick_path, MAX_DPATH, "%s/Kickstarts", save_path);

	// LOAD GAME SPECIFICS
	_sntprintf(whd_path, MAX_DPATH, "%sgame-data/", whdboot_path);
	game_options game_detail;

	_tcscpy(whd_config, whd_path);
	_tcscat(whd_config, "whdload_db.xml");

	if (my_existsfile2(whd_config))
	{
		game_detail = parse_settings_from_xml(prefs, filepath);
	}
	else
	{
		write_log("WHDBooter -  Could not load whdload_db.xml - does not exist?\n");
	}

	// If we have a slave, create a startup-sequence
	if (strlen(selected_slave) != 0)
	{
		create_startup_sequence(prefs);
	}

	// now we should have a startup-sequence file (if we don't, we are going to use the original booter)
	if (my_existsfile2(whd_startup))
	{
		// create a symlink to WHDLoad in /tmp/amiberry/
		_sntprintf(whd_path, MAX_DPATH, "%sWHDLoad", whdboot_path);
		symlink(whd_path, "/tmp/amiberry/c/WHDLoad");

		// Create a symlink to AmiQuit in /tmp/amiberry/
		_sntprintf(whd_path, MAX_DPATH, "%sAmiQuit", whdboot_path);
		symlink(whd_path, "/tmp/amiberry/c/AmiQuit");

		// create a symlink for DEVS in /tmp/amiberry/
		symlink(kick_path, "/tmp/amiberry/devs/Kickstarts");
	}
#if DEBUG
	// debugging code!
	write_log("WHDBooter - Game: Port 0     : %s  \n", game_detail.port0.c_str());
	write_log("WHDBooter - Game: Port 1     : %s  \n", game_detail.port1.c_str());
	write_log("WHDBooter - Game: Control    : %s  \n", game_detail.control.c_str());
	write_log("WHDBooter - Game: CPU        : %s  \n", game_detail.cpu.c_str());
	write_log("WHDBooter - Game: Blitter    : %s  \n", game_detail.blitter.c_str());
	write_log("WHDBooter - Game: CPU Clock  : %s  \n", game_detail.clock.c_str());
	write_log("WHDBooter - Game: Chipset    : %s  \n", game_detail.chipset.c_str());
	write_log("WHDBooter - Game: JIT        : %s  \n", game_detail.jit.c_str());
	write_log("WHDBooter - Game: CPU Compat : %s  \n", game_detail.cpu_comp.c_str());
	write_log("WHDBooter - Game: Sprite Col : %s  \n", game_detail.sprites.c_str());
	write_log("WHDBooter - Game: Scr Height : %s  \n", game_detail.scr_height.c_str());
	write_log("WHDBooter - Game: Scr Width  : %s  \n", game_detail.scr_width.c_str());
	write_log("WHDBooter - Game: Scr AutoHgt: %s  \n", game_detail.scr_autoheight.c_str());
	write_log("WHDBooter - Game: Scr CentrH : %s  \n", game_detail.scr_centerh.c_str());
	write_log("WHDBooter - Game: Scr CentrV : %s  \n", game_detail.scr_centerv.c_str());
	write_log("WHDBooter - Game: Scr OffsetH: %s  \n", game_detail.scr_offseth.c_str());
	write_log("WHDBooter - Game: Scr OffsetV: %s  \n", game_detail.scr_offsetv.c_str());
	write_log("WHDBooter - Game: NTSC       : %s  \n", game_detail.ntsc.c_str());
	write_log("WHDBooter - Game: Fast Ram   : %s  \n", game_detail.fast.c_str());
	write_log("WHDBooter - Game: Z3 Ram     : %s  \n", game_detail.z3.c_str());
	write_log("WHDBooter - Game: CPU Exact  : %s  \n", game_detail.cpu_exact.c_str());
	write_log("WHDBooter - Game: Fast Copper: %s  \n", game_detail.fastcopper.c_str());

	write_log("WHDBooter - Host: Controller 1   : %s  \n", amiberry_options.default_controller1);
	write_log("WHDBooter - Host: Controller 2   : %s  \n", amiberry_options.default_controller2);
	write_log("WHDBooter - Host: Controller 3   : %s  \n", amiberry_options.default_controller3);
	write_log("WHDBooter - Host: Controller 4   : %s  \n", amiberry_options.default_controller4);
	write_log("WHDBooter - Host: Mouse 1        : %s  \n", amiberry_options.default_mouse1);
	write_log("WHDBooter - Host: Mouse 2        : %s  \n", amiberry_options.default_mouse2);
#endif

	// if we already loaded a .uae config, we don't need to do the below manual setup for hardware
	if (my_existsfile2(uae_config))
	{
		write_log("WHDBooter - %s found; ignoring WHD Quickstart setup.\n", uae_config);
		return;
	}

	//    *** EMULATED HARDWARE ***
	//    SET UNIVERSAL DEFAULTS
	prefs->start_gui = false;

	// DO CHECKS FOR AGA / CD32
	const auto is_aga = _tcsstr(filename, "AGA") != nullptr || strcmpi(game_detail.chipset.c_str(), "AGA") == 0;
	const auto is_cd32 = _tcsstr(filename, "CD32") != nullptr || strcmpi(game_detail.chipset.c_str(), "CD32") == 0;

	if (is_aga || is_cd32 || !a600_available)
	{
		// SET THE BASE AMIGA (Expanded A1200)
		built_in_prefs(prefs, 4, A1200_CONFIG, 0, 0);
		_tcscpy(prefs->description, _T("AutoBoot Configuration [WHDLoad] [AGA]"));
	}
	else
	{
		// SET THE BASE AMIGA (Expanded A600)
		built_in_prefs(prefs, 2, A600_CONFIG, 0, 0);
		_tcscpy(prefs->description, _T("AutoBoot Configuration [WHDLoad]"));
	}

	// SET THE WHD BOOTER AND GAME DATA
	set_booter_drives(prefs, filepath);

	// APPLY THE SETTINGS FOR MOUSE/JOYSTICK ETC
	set_input_settings(prefs, game_detail, is_cd32);

	//  SET THE GAME COMPATIBILITY SETTINGS
	// BLITTER, SPRITES, MEMORY, JIT, BIG CPU ETC
	set_compatibility_settings(prefs, game_detail, a600_available, is_aga || is_cd32);
}
