#include "global.h"

// ============================================================
// Free Library - browse and download free PBR materials,
// models and HDRIs straight into ArmorPaint. Sources:
//   Poly Haven, ambientCG, poly.pizza, Sketchfab, Blendkit.
// Files are saved into the material library folder and
// imported into the open project immediately.
//
// Open the "Plugins" tab to use it.
// ============================================================

void *plugin;

// ---- configuration ----
// All of this can be set in a .env file instead (see .env.example):
// place it in the ArmorPaint data folder (data/.env) or next to the
// executable (./.env). Values are read once at startup - restart
// ArmorPaint after editing. Defaults below apply when .env is absent.
//
// where downloads are stored
char *LIB_ROOT  = "./FreeLibrary";
char *THUMB_DIR = "./.freelib_thumbs";

// API keys (both free, self-service)
// poly.pizza key - register at https://poly.pizza/settings/api
char *POLY_PIZZA_KEY = "";
// Sketchfab API token - your account settings, Password -> API token
// (https://sketchfab.com/settings/password)
char *SKETCHFAB_TOKEN = "";

// ---- set to 1 to run an automated headless test on startup ----
// 1=PH material 2=acg material 3=PH HDRI 4=PH model
// 5=poly.pizza model 6=Sketchfab model 7=Blendkit model 8=Blendkit HDRI
int selftest = 0;

// ---- UI state ----
bool expanded  = true;
int  src_sel   = 0; // 0 Poly Haven, 1 ambientCG, 2 poly.pizza, 3 Sketchfab, 4 Blendkit
int  type_sel  = 0; // 0 Materials, 1 Models, 2 HDRIs
int  qual_sel  = 1; // 0 1K, 1 2K, 2 4K (Sketchfab: 0 CC0, 1 CC-BY, 2 CC-BY-SA)
char *search   = "";
int  page      = 0;
int  have_results = 0;
int  first_ui  = 1;
int  st_pending = 0; // selftest marker, written on the next frame

char *status     = "Free Library ready.";
char *page_label = "";

// ---- result page (parallel arrays) ----
string_array_t *res_ids        = NULL;
string_array_t *res_page_ids   = NULL; // ids of the displayed page
string_array_t *res_disp       = NULL; // "Name  [id]" rows
string_array_t *res_thumbs     = NULL; // local thumb path or ""
string_array_t *res_thumb_urls = NULL; // remote thumb url or ""
int res_total  = 0;
int page_count = 1;

// ---- network op: 0 idle, 1 search json, 2 files json, 3 asset file, 4 thumb ----
int  op         = 0;
char *ph_json   = "";
char *ph_json_tex = "";
char *ph_json_mdl = "";
char *ph_json_hdr = "";
char *acg_json  = "";
int  live_src   = -1;
int  live_type  = -1;
int  st_kick    = 0;

// ---- thumb prefetch ----
int thumb_i       = 0;
int thumb_inflight = -1;

// ---- asset download queue ----
string_array_t *dl_urls = NULL;
string_array_t *dl_dsts = NULL;
int  dl_i      = 0;
int  dl_busy   = 0;
int  dl_kick   = 0;
int  dl_zip    = 0;
int  dl_hdr    = 0;
char *dl_kind  = "";
char *dl_dir   = "";
char *dl_primary = "";
char *dl_zipfile = "";
char *dl_id    = "";
char *dl_pending_id = "";
char *ph_files_json = "";
char *ph_files_id   = "";
char *acg_files_json = "";
char *acg_files_id   = "";

// ---- server-paged sources ----
char *pp_json  = ""; // poly.pizza search response
char *sf_json  = ""; // Sketchfab search response
char *bk_json  = ""; // Blendkit search response
int  dl_src    = -1; // source captured when a download starts
int  dl_type   = 0;  // type captured when a download starts
string_array_t *page_curs = NULL; // cursor per page for poly.pizza / Sketchfab

// ---- combo/row data, created once in main ----
string_array_t *src_items  = NULL;
string_array_t *type_items = NULL;
string_array_t *qual_items = NULL;
string_array_t *sf_lic_items = NULL;
f32_array_t *row3       = NULL;
f32_array_t *row_search = NULL;
f32_array_t *row_res    = NULL;

// ---- helpers ----

void set_status(char *s) {
	status = string("%s", s); // string() results persist across frames, literals don't
	console_info(string("[FreeLibrary] %s", s));
}

void st_marker() {
	// Deferred one frame: the test driver deletes stale markers in its main(),
	// which runs after plugin main - writing directly would race with that.
	if (selftest) {
		st_pending = 1;
	}
}

char *base_name(char *path) {
	int i = string_last_index_of(path, "/");
	if (i < 0) {
		return path;
	}
	return substring(path, i + 1, string_length(path));
}

void ensure_parent(char *path) {
	int i = string_last_index_of(path, "/");
	if (i > 0) {
		iron_create_directory(substring(path, 0, i));
	}
}

int str_contains(char *hay, char *needle) {
	int hl = string_length(hay);
	int nl = string_length(needle);
	if (nl == 0) {
		return 1;
	}
	if (nl > hl) {
		return 0;
	}
	for (int i = 0; i <= hl - nl; i++) {
		int  ok = 1;
		int  j  = 0;
		for (j = 0; j < nl; j++) {
			if (char_code_at(hay, i + j) != char_code_at(needle, j)) {
				ok = 0;
				break;
			}
		}
		if (ok) {
			return 1;
		}
	}
	return 0;
}

int str_starts(char *s, char *p) {
	int nl = string_length(p);
	if (string_length(s) < nl) {
		return 0;
	}
	for (int i = 0; i < nl; i++) {
		if (char_code_at(s, i) != char_code_at(p, i)) {
			return 0;
		}
	}
	return 1;
}

char *urlencode(char *s) {
	s = string_replace_all(s, " ", "%20");
	s = string_replace_all(s, "&", "%26");
	s = string_replace_all(s, "#", "%23");
	s = string_replace_all(s, "+", "%2B");
	return s;
}

// file extension from a URL, query string stripped (".glb", ".exr", ...)
char *url_file_ext(char *url) {
	int q = string_last_index_of(url, "?");
	if (q >= 0) {
		url = substring(url, 0, q);
	}
	int d = string_last_index_of(url, ".");
	int s = string_last_index_of(url, "/");
	if (d > s && d >= 0) {
		return substring(url, d, string_length(url));
	}
	return string(".bin");
}

int src_supports_type(int t) {
	if (src_sel == 0) {
		return 1; // Poly Haven: materials, models, HDRIs
	}
	if (src_sel == 1) {
		return t != 1; // ambientCG: no model search
	}
	if (src_sel == 4) {
		return t != 0; // Blendkit: models + HDRIs (materials are .blend only)
	}
	return t == 1; // poly.pizza, Sketchfab: models only
}

void write_credits(char *dir, char *text) {
	iron_file_save_bytes(string("%s/credits.txt", dir), sys_string_to_buffer(text), string_length(text));
}

void dl_push(char *url) {
	string_array_push(dl_urls, url);
	string_array_push(dl_dsts, string("%s/%s", dl_dir, base_name(url)));
}

// ---- results ----

void fill_page() {
	res_page_ids = string_array_create(0);
	res_disp   = string_array_create(0);
	res_thumbs = string_array_create(0);
	res_thumb_urls = string_array_create(0);

	int start = 0;
	int count = 0;
	if (src_sel == 0) {
		start = page * 10;
		count = res_total - start;
		if (count > 10) {
			count = 10;
		}
		if (count < 0) {
			count = 0;
		}
	}
	else {
		count = res_ids->length; // server-paged sources: res_ids already holds only this page
	}

	for (int i = 0; i < count; i++) {
		int   idx = start + i;
		if (idx >= res_ids->length) {
			break;
		}
		char *id   = res_ids->buffer[idx];
		char *name = NULL;
		char *thumb = string("");
		char *turl  = string("");
		if (src_sel == 0) {
			name = json_get(ph_json, string("%s.name", id));
			thumb = string("%s/ph_%s.jpg", THUMB_DIR, id);
			turl  = string("https://cdn.polyhaven.com/asset_img/primary/%s.png?height=100", id);
		}
		else if (src_sel == 1) {
			name = json_get(acg_json, string("foundAssets.%d.displayName", i));
			thumb = string("%s/acg_%s.jpg", THUMB_DIR, id);
			turl  = json_get(acg_json, string("foundAssets.%d.previewImage.256-JPG-FFFFFF", i));
		}
		else if (src_sel == 2) {
			name = json_get(pp_json, string("results.%d.Title", i));
			thumb = string("%s/pp_%s.jpg", THUMB_DIR, id);
			turl  = json_get(pp_json, string("results.%d.Thumbnail", i));
		}
		else if (src_sel == 3) {
			name = json_get(sf_json, string("results.%d.name", i));
			thumb = string("%s/sf_%s.jpg", THUMB_DIR, id);
			char *imgs = string("results.%d.thumbnails.images", i);
			int   n    = json_len(sf_json, imgs);
			int   pick = 3; // 0..2 large, 3 = 256px, 4 = 64px
			if (pick >= n) {
				pick = n - 1;
			}
			if (pick >= 0) {
				turl = json_get(sf_json, string("%s.%d.url", imgs, pick));
			}
		}
		else {
			name = json_get(bk_json, string("results.%d.displayName", i));
			thumb = string("%s/bk_%s.jpg", THUMB_DIR, id);
			turl  = json_get(bk_json, string("results.%d.thumbnailMiddleUrl", i));
		}
		if (name == NULL) {
			name = id;
		}
		if (turl == NULL) {
			turl = string("");
		}
		string_array_push(res_page_ids, id);
		string_array_push(res_disp, string("%s   %s", name, id));
		string_array_push(res_thumbs, thumb);
		string_array_push(res_thumb_urls, turl);
	}
	thumb_i = 0;
	thumb_inflight = -1;
}

void apply_ph_results() {
	res_ids = json_filter(ph_json, "", search, 0);
	res_total = res_ids->length;
	page_count = (res_total + 9) / 10;
	if (page_count < 1) {
		page_count = 1;
	}
	if (page >= page_count) {
		page = page_count - 1;
	}
	if (page < 0) {
		page = 0;
	}
	page_label = string("Page %d/%d - %d found", page + 1, page_count, res_total);
	fill_page();
	have_results = 1;
	set_status(string("%d matches for '%s'", res_total, search));
}

void apply_acg_results() {
	res_ids = string_array_create(0);
	int n = json_len(acg_json, "foundAssets");
	for (int i = 0; i < n; i++) {
		char *id = json_get(acg_json, string("foundAssets.%d.assetId", i));
		if (id != NULL) {
			string_array_push(res_ids, id);
		}
	}
	res_total = res_ids->length;
	char *total = json_get(acg_json, "numberOfResults");
	if (total == NULL) {
		total = string("%d", res_total);
	}
	page_label = string("Page %d - %s total", page + 1, total);
	fill_page();
	have_results = 1;
	set_status(string("Showing %d results (page %d)", res_total, page + 1));
}

// server-paged sources: res_ids already holds this page's ids
void apply_page_results(char *label_total) {
	page_label = string("Page %d - %d shown", page + 1, res_total);
	if (src_sel == 2 || src_sel == 3) {
		if (page_curs != NULL) {
			if (page + 1 < page_curs->length) {
				page_label = string("Page %d - %d shown, more below", page + 1, res_total);
			}
		}
	}
	fill_page();
	have_results = 1;
	if (label_total != NULL) {
		set_status(string("%d shown (page %d), %s", res_total, page + 1, label_total));
	}
	else {
		set_status(string("%d shown (page %d)", res_total, page + 1));
	}
}

void apply_pp_results() {
	res_ids = string_array_create(0);
	int n = json_len(pp_json, "results");
	for (int i = 0; i < n; i++) {
		char *id = json_get(pp_json, string("results.%d.ID", i));
		if (id != NULL) {
			string_array_push(res_ids, id);
		}
	}
	res_total = res_ids->length;
	apply_page_results(NULL);
}

void apply_sf_results() {
	res_ids = string_array_create(0);
	int n = json_len(sf_json, "results");
	for (int i = 0; i < n; i++) {
		char *id = json_get(sf_json, string("results.%d.uid", i));
		if (id != NULL) {
			string_array_push(res_ids, id);
		}
	}
	res_total = res_ids->length;
	apply_page_results(NULL);
}

void apply_bk_results() {
	res_ids = string_array_create(0);
	int n = json_len(bk_json, "results");
	for (int i = 0; i < n; i++) {
		char *id = json_get(bk_json, string("results.%d.id", i));
		if (id != NULL) {
			string_array_push(res_ids, id);
		}
	}
	res_total = res_ids->length;
	apply_page_results(NULL);
}

void on_search_json(char *url, buffer_t *buf) {
	if (op == 1) {
		op = 0;
	}
	if (buf == NULL) {
		set_status("Search failed - check internet connection");
		st_marker();
		return;
	}
	char *s = sys_buffer_to_string(buf);
	free(buf->buffer);
	free(buf);
	if (str_starts(s, "{\"error") || str_starts(s, "{\"detail") || str_starts(s, "<!DOC") || str_starts(s, "<html")) {
		char *msg = json_get(s, "error.message");
		if (msg == NULL) {
			msg = json_get(s, "detail");
		}
		if (msg == NULL) {
			int sl = string_length(s);
			if (sl > 90) {
				sl = 90;
			}
			msg = substring(s, 0, sl);
		}
		set_status(string("API error: %s", msg));
		st_marker();
		return;
	}
	if (src_sel == 0) {
		if (type_sel == 0) {
			ph_json_tex = s;
		}
		if (type_sel == 1) {
			ph_json_mdl = s;
		}
		if (type_sel == 2) {
			ph_json_hdr = s;
		}
		ph_json = s;
		apply_ph_results();
	}
	else if (src_sel == 1) {
		acg_json = s;
		apply_acg_results();
	}
	else {
		// remember the cursor for the next page (poly.pizza / Sketchfab)
		if (src_sel == 2 || src_sel == 3) {
			if (page_curs != NULL) {
				if (page_curs->length == page + 1) {
					char *next = json_get(s, "cursor");
					if (src_sel == 3) {
						next = json_get(s, "cursors.next");
					}
					if (next != NULL && !string_equals(next, "") && !string_equals(next, "null")) {
						string_array_push(page_curs, next);
					}
				}
			}
		}
		if (src_sel == 2) {
			pp_json = s;
			apply_pp_results();
		}
		else if (src_sel == 3) {
			sf_json = s;
			apply_sf_results();
		}
		else {
			bk_json = s;
			apply_bk_results();
		}
	}
	if (selftest) {
		st_kick = 1;
	}
}

// ---- thumbnails ----

void on_thumb_done(char *url) {
	if (op == 4) {
		op = 0;
		if (thumb_inflight >= 0) {
			if (thumb_i == thumb_inflight) {
				thumb_i = thumb_i + 1;
			}
			thumb_inflight = -1;
		}
	}
}

void pump_thumbs() {
	if (op != 0 || dl_busy || !have_results) {
		return;
	}
	while (thumb_i < res_thumbs->length) {
		char *thumb = res_thumbs->buffer[thumb_i];
		char *turl  = res_thumb_urls->buffer[thumb_i];
		if (string_equals(turl, "") || (iron_file_exists(thumb) && iron_file_size(thumb) > 0)) {
			thumb_i = thumb_i + 1;
			continue;
		}
		thumb_inflight = thumb_i;
		op = 4;
		script_file_download_to(turl, thumb, on_thumb_done, 0);
		return;
	}
}

// ---- asset download ----

void finish_download() {
	dl_busy = 0;
	if (dl_zip) {
		char *skip = "NormalDX;.blend;.usdc;.mtlx;.tres;.png";
		if (dl_type == 1) {
			skip = "NormalDX;.usdc;.tres;.blend";
		}
		int n = zip_extract(dl_zipfile, dl_dir, skip);
		if (n <= 0) {
			set_status(string("Extract failed: %s", base_name(dl_zipfile)));
			return;
		}
		iron_delete_file(dl_zipfile); // extracted - archive no longer needed
	}
	int ok = 0;
	if (string_equals(dl_kind, "folder")) {
		script_import_asset(dl_dir, 0);
		ok = 1;
	}
	else if (string_equals(dl_kind, "primary")) {
		script_import_asset(dl_primary, dl_hdr);
		ok = 1;
	}
	else {
		string_array_t *files = file_read_directory(dl_dir);
		for (int i = 0; i < files->length; i++) {
			char *f = files->buffer[i];
			int   found = 0;
			if (string_equals(dl_kind, "find_mesh")) {
				if (ends_with(f, ".gltf") || ends_with(f, ".glb") || ends_with(f, ".obj") || ends_with(f, ".fbx")) {
					found = 1;
				}
			}
			else {
				if (ends_with(f, ".hdr") || ends_with(f, ".exr")) {
					found = 1;
				}
			}
			if (found) {
				script_import_asset(string("%s/%s", dl_dir, f), dl_hdr);
				ok = 1;
				break;
			}
		}
	}
	if (ok) {
		set_status(string("Imported: %s", dl_id));
	}
	else {
		set_status(string("Import failed: %s", dl_id));
	}
	if (selftest) {
		console_info("SELFTEST IMPORT DONE");
		st_marker();
	}
}

void on_dl_done(char *url) {
	if (op == 3) {
		op = 0;
		dl_kick = 1;
	}
}

void next_dl() {
	if (dl_urls == NULL) {
		dl_busy = 0;
		return;
	}
	while (dl_i < dl_urls->length) {
		char *url = dl_urls->buffer[dl_i];
		char *dst = dl_dsts->buffer[dl_i];
		if (iron_file_exists(dst) && iron_file_size(dst) > 0) {
			dl_i = dl_i + 1;
			continue;
		}
		ensure_parent(dst);
		op = 3;
		set_status(string("Downloading %d/%d: %s", dl_i + 1, dl_urls->length, base_name(dst)));
		script_file_download_to(url, dst, on_dl_done, 0);
		return;
	}
	finish_download();
}

void build_ph_queue(char *json, char *id) {
	char *q = "1k";
	if (qual_sel == 1) {
		q = "2k";
	}
	if (qual_sel == 2) {
		q = "4k";
	}
	dl_urls = string_array_create(0);
	dl_dsts = string_array_create(0);
	dl_zip  = 0;
	dl_hdr  = 0;
	dl_primary = string("");
	dl_zipfile = string("");

	if (type_sel == 0) {
		// PBR material: download map files directly
		dl_kind = string("folder");
		char *u = json_get(json, string("Diffuse.%s.jpg.url", q));
		if (u == NULL) {
			u = json_get(json, string("Color.%s.jpg.url", q));
		}
		if (u == NULL) {
			set_status(string("No diffuse map at %s: %s", q, id));
			dl_busy = 0;
			return;
		}
		dl_push(u);
		u = json_get(json, string("nor_gl.%s.jpg.url", q));
		if (u == NULL) {
			u = json_get(json, string("nor_dx.%s.jpg.url", q));
		}
		if (u != NULL) {
			dl_push(u);
		}
		u = json_get(json, string("Rough.%s.jpg.url", q));
		if (u == NULL) {
			u = json_get(json, string("rough_ao.%s.jpg.url", q));
		}
		if (u != NULL) {
			dl_push(u);
		}
		u = json_get(json, string("AO.%s.jpg.url", q));
		if (u != NULL) {
			dl_push(u);
		}
		u = json_get(json, string("Metal.%s.jpg.url", q));
		if (u != NULL) {
			dl_push(u);
		}
		u = json_get(json, string("Displacement.%s.jpg.url", q));
		if (u != NULL) {
			dl_push(u);
		}
	}
	else if (type_sel == 1) {
		// Model: .gltf + included files (textures, buffer)
		dl_kind = string("primary");
		char *rq = q;
		if (json_get(json, string("gltf.%s.gltf.url", rq)) == NULL) {
			rq = "1k";
		}
		char *u = json_get(json, string("gltf.%s.gltf.url", rq));
		if (u == NULL) {
			set_status(string("No glTF at %s: %s", rq, id));
			dl_busy = 0;
			return;
		}
		dl_primary = string("%s/%s", dl_dir, base_name(u));
		dl_push(u);
		string_array_t *keys = json_keys(json, string("gltf.%s.gltf.include", rq));
		for (int i = 0; i < keys->length; i++) {
			char *k  = keys->buffer[i];
			char *ek = string_replace_all(k, ".", "\\.");
			char *u2 = json_get(json, string("gltf.%s.gltf.include.%s.url", rq, ek));
			if (u2 != NULL) {
				string_array_push(dl_urls, u2);
				string_array_push(dl_dsts, string("%s/%s", dl_dir, k));
			}
		}
	}
	else {
		// HDRI
		dl_kind = string("primary");
		dl_hdr  = 1;
		char *u = json_get(json, string("hdri.%s.hdr.url", q));
		if (u == NULL) {
			u = json_get(json, string("hdri.%s.exr.url", q));
		}
		if (u == NULL) {
			set_status(string("No HDRI at %s: %s", q, id));
			dl_busy = 0;
			return;
		}
		dl_primary = string("%s/%s", dl_dir, base_name(u));
		dl_push(u);
	}

	dl_i = 0;
	if (dl_urls->length == 0) {
		dl_busy = 0;
		set_status("Nothing to download");
		return;
	}
	next_dl();
}

void build_acg_queue(char *json, char *id) {
	char *q = "1K";
	if (qual_sel == 1) {
		q = "2K";
	}
	if (qual_sel == 2) {
		q = "4K";
	}
	dl_urls = string_array_create(0);
	dl_dsts = string_array_create(0);
	dl_zip  = 1;
	dl_hdr  = 0;
	dl_primary = string("");
	dl_zipfile = string("");

	char *base = "foundAssets.0.downloadFolders.default.downloadFiletypeCategories.zip.downloads";
	int   n    = json_len(json, base);
	if (n == 0) {
		set_status(string("No zip downloads for %s", id));
		dl_busy = 0;
		return;
	}
	int pick     = -1;
	int fallback = -1;
	for (int i = 0; i < n; i++) {
		char *attr = json_get(json, string("%s.%d.attribute", base, i));
		if (attr == NULL) {
			continue;
		}
		if (fallback < 0) {
			fallback = i;
		}
		if (type_sel == 0) {
			if (string_equals(attr, string("%s-JPG", q))) {
				pick = i;
				break;
			}
		}
		else {
			if (str_contains(attr, q)) {
				pick = i;
				break;
			}
		}
	}
	if (pick < 0) {
		pick = fallback;
	}
	char *u     = json_get(json, string("%s.%d.downloadLink", base, pick));
	char *fname = json_get(json, string("%s.%d.fileName", base, pick));
	if (u == NULL) {
		set_status(string("No download link for %s", id));
		dl_busy = 0;
		return;
	}
	if (fname == NULL) {
		fname = string("%s_%s.zip", id, q);
	}
	string_array_push(dl_urls, u);
	string_array_push(dl_dsts, string("%s/%s", dl_dir, fname));
	dl_zipfile = string("%s/%s", dl_dir, fname);

	if (type_sel == 0) {
		dl_kind = string("folder");
	}
	else if (type_sel == 1) {
		dl_kind = string("find_mesh");
	}
	else {
		dl_kind = string("find_hdr");
		dl_hdr  = 1;
	}

	dl_i = 0;
	next_dl();
}

// ---- poly.pizza / Sketchfab / Blendkit queue builders ----

int find_res_index(char *json, char *id, char *key_fmt) {
	int n = json_len(json, "results");
	for (int i = 0; i < n; i++) {
		char *rid = json_get(json, string(key_fmt, i));
		if (rid != NULL && string_equals(rid, id)) {
			return i;
		}
	}
	return -1;
}

void build_pp_queue(char *id) {
	dl_urls = string_array_create(0);
	dl_dsts = string_array_create(0);
	dl_zip  = 0;
	dl_hdr  = 0;
	dl_primary = string("");
	dl_zipfile = string("");

	int idx = find_res_index(pp_json, id, "results.%d.ID");
	if (idx < 0) {
		set_status(string("Result not found: %s", id));
		dl_busy = 0;
		st_marker();
		return;
	}
	char *u = json_get(pp_json, string("results.%d.Download", idx));
	if (u == NULL) {
		set_status(string("No download URL for %s", id));
		dl_busy = 0;
		st_marker();
		return;
	}
	dl_kind    = string("primary");
	dl_primary = string("%s/%s%s", dl_dir, id, url_file_ext(u));
	string_array_push(dl_urls, u);
	string_array_push(dl_dsts, dl_primary);

	char *lic = json_get(pp_json, string("results.%d.Licence", idx));
	if (lic != NULL) {
		char *who   = json_get(pp_json, string("results.%d.Creator.Username", idx));
		char *title = json_get(pp_json, string("results.%d.Title", idx));
		if (title == NULL) {
			title = id;
		}
		if (who == NULL) {
			who = string("unknown");
		}
		write_credits(dl_dir, string("%s by %s - %s - https://poly.pizza/model/%s\n", title, who, lic, id));
	}

	dl_i = 0;
	if (dl_urls->length == 0) {
		dl_busy = 0;
		set_status("Nothing to download");
		st_marker();
		return;
	}
	next_dl();
}

// Sketchfab: second step - download-info JSON already fetched into s
void build_sf_queue(char *s, char *id) {
	dl_urls = string_array_create(0);
	dl_dsts = string_array_create(0);
	dl_zip  = 1;
	dl_hdr  = 0;
	dl_primary = string("");
	dl_zipfile = string("");

	char *u = json_get(s, "gltf.url");
	if (u == NULL) {
		char *msg = json_get(s, "detail");
		if (msg == NULL) {
			msg = json_get(s, "error");
		}
		if (msg != NULL) {
			set_status(string("Sketchfab: %s", msg));
		}
		else {
			set_status("Sketchfab download link failed - check SKETCHFAB_TOKEN");
		}
		dl_busy = 0;
		st_marker();
		return;
	}
	dl_kind     = string("find_mesh");
	dl_zipfile  = string("%s/%s.zip", dl_dir, id);
	string_array_push(dl_urls, u);
	string_array_push(dl_dsts, dl_zipfile);

	int sidx = find_res_index(sf_json, id, "results.%d.uid");
	if (sidx >= 0) {
		char *lic = json_get(sf_json, string("results.%d.license.label", sidx));
		if (lic != NULL) {
			char *who  = json_get(sf_json, string("results.%d.user.username", sidx));
			char *link = json_get(sf_json, string("results.%d.viewerUrl", sidx));
			char *nm   = json_get(sf_json, string("results.%d.name", sidx));
			if (who == NULL) {
				who = string("unknown");
			}
			if (link == NULL) {
				link = string("https://sketchfab.com/3d-models/%s", id);
			}
			if (nm == NULL) {
				nm = id;
			}
			write_credits(dl_dir, string("%s by %s - %s - %s\n", nm, who, lic, link));
		}
	}

	dl_i = 0;
	if (dl_urls->length == 0) {
		dl_busy = 0;
		set_status("Nothing to download");
		st_marker();
		return;
	}
	next_dl();
}

// Blendkit: second step - meta JSON in s gives the signed CDN URL
void finish_bk_queue(char *s) {
	dl_urls = string_array_create(0);
	dl_dsts = string_array_create(0);
	dl_zip  = 0;
	dl_primary = string("");
	dl_zipfile = string("");

	char *u = json_get(s, "filePath");
	if (u == NULL) {
		set_status("Blendkit download link failed");
		dl_busy = 0;
		st_marker();
		return;
	}
	dl_kind    = string("primary");
	dl_primary = string("%s/%s%s", dl_dir, dl_pending_id, url_file_ext(u));
	string_array_push(dl_urls, u);
	string_array_push(dl_dsts, dl_primary);

	dl_i = 0;
	next_dl();
}

void on_files_json(char *url, buffer_t *buf) {
	if (op == 2) {
		op = 0;
	}
	if (buf == NULL) {
		set_status("Failed to fetch file metadata");
		dl_busy = 0;
		st_marker();
		return;
	}
	char *s = sys_buffer_to_string(buf);
	free(buf->buffer);
	free(buf);
	if (dl_src == 0) {
		ph_files_json = s;
		ph_files_id   = dl_pending_id;
		build_ph_queue(s, dl_pending_id);
	}
	else if (dl_src == 1) {
		acg_files_json = s;
		acg_files_id   = dl_pending_id;
		build_acg_queue(s, dl_pending_id);
	}
	else if (dl_src == 3) {
		build_sf_queue(s, dl_pending_id);
	}
	else {
		finish_bk_queue(s);
	}
}

// Blendkit: first step - resolve the file's signed CDN URL
void bk_begin(char *id) {
	int idx = find_res_index(bk_json, id, "results.%d.id");
	if (idx < 0) {
		set_status(string("Result not found: %s", id));
		dl_busy = 0;
		st_marker();
		return;
	}

	char *want = "gltf"; // models
	if (dl_type == 2) {
		want = "resolution_2K";
		if (qual_sel == 0) {
			want = "resolution_1K";
		}
		if (qual_sel == 2) {
			want = "resolution_4K";
		}
	}
	char *fu = NULL;
	char *fb = NULL;
	int   fn = json_len(bk_json, string("results.%d.files", idx));
	for (int f = 0; f < fn; f++) {
		char *ft = json_get(bk_json, string("results.%d.files.%d.fileType", idx, f));
		if (ft == NULL) {
			continue;
		}
		if (string_equals(ft, want)) {
			fu = json_get(bk_json, string("results.%d.files.%d.downloadUrl", idx, f));
			break;
		}
		// fallback: any gltf / any resolution file
		if (fb == NULL && string_equals(ft, "gltf")) {
			fb = json_get(bk_json, string("results.%d.files.%d.downloadUrl", idx, f));
		}
		if (fb == NULL && dl_type == 2 && str_starts(ft, "resolution")) {
			fb = json_get(bk_json, string("results.%d.files.%d.downloadUrl", idx, f));
		}
	}
	if (fu == NULL) {
		fu = fb;
	}
	if (fu == NULL) {
		set_status(string("No usable file for: %s", id));
		dl_busy = 0;
		st_marker();
		return;
	}

	char *lic = json_get(bk_json, string("results.%d.license", idx));
	if (lic != NULL) {
		char *f1  = json_get(bk_json, string("results.%d.author.firstName", idx));
		char *f2  = json_get(bk_json, string("results.%d.author.lastName", idx));
		char *nm  = json_get(bk_json, string("results.%d.displayName", idx));
		char *who = string("unknown");
		if (f1 != NULL) {
			if (f2 != NULL) {
				who = string("%s %s", f1, f2);
			}
			else {
				who = f1;
			}
		}
		if (nm == NULL) {
			nm = id;
		}
		write_credits(dl_dir, string("%s by %s - %s - https://www.blendkit.com/\n", nm, who, lic));
	}

	op = 2;
	set_status("Fetching Blendkit download link...");
	script_file_download(string("%s?scene_uuid=11111111-2222-3333-4444-555555555555", fu), NULL, on_files_json, 0, NULL);
}

void start_download(char *id) {
	if (dl_busy) {
		set_status("Already downloading - please wait");
		return;
	}
	if (op == 4) {
		op = 0; // cancel thumb prefetch, it resumes later
		thumb_inflight = -1;
	}
	if (op != 0) {
		set_status("Busy - try again in a moment");
		return;
	}
	dl_busy = 1;
	dl_i    = 0;
	dl_id   = id;
	dl_pending_id = id;
	dl_src  = src_sel;
	dl_type = type_sel;
	dl_zip  = 0;
	dl_hdr  = 0;
	dl_kind = string("");
	dl_primary = string("");
	dl_zipfile = string("");

	char *srcname = "PolyHaven";
	if (src_sel == 1) {
		srcname = "ambientCG";
	}
	if (src_sel == 2) {
		srcname = "poly.pizza";
	}
	if (src_sel == 3) {
		srcname = "Sketchfab";
	}
	if (src_sel == 4) {
		srcname = "Blendkit";
	}
	dl_dir = string("%s/%s/%s", LIB_ROOT, srcname, id);
	iron_create_directory(LIB_ROOT);
	iron_create_directory(string("%s/%s", LIB_ROOT, srcname));
	iron_create_directory(dl_dir);
	set_status(string("Preparing %s...", id));

	if (dl_src == 0) {
		if (string_equals(ph_files_id, id) && !string_equals(ph_files_json, "")) {
			build_ph_queue(ph_files_json, id);
		}
		else {
			op = 2;
			script_file_download(string("https://api.polyhaven.com/files/%s", id), NULL, on_files_json, 0, NULL);
		}
	}
	else if (dl_src == 1) {
		if (string_equals(acg_files_id, id) && !string_equals(acg_files_json, "")) {
			build_acg_queue(acg_files_json, id);
		}
		else {
			op = 2;
			script_file_download(string("https://ambientcg.com/api/v2/full_json?id=%s&include=downloadData", id), NULL, on_files_json, 0, NULL);
		}
	}
	else if (dl_src == 2) {
		build_pp_queue(id);
	}
	else if (dl_src == 3) {
		if (string_equals(SKETCHFAB_TOKEN, "")) {
			set_status("Set SKETCHFAB_TOKEN in data/.env (sketchfab.com/settings -> API token)");
			dl_busy = 0;
			st_marker();
			return;
		}
		op = 2;
		script_file_download(string("https://api.sketchfab.com/v3/models/%s/download", id), string("Authorization: Token %s\r\n", SKETCHFAB_TOKEN), on_files_json, 0,
		                     NULL);
	}
	else {
		bk_begin(id);
	}
}

// ---- search ----

void fetch_results() {
	if (op == 4) {
		op = 0;
		thumb_inflight = -1;
	}
	if (op != 0 || dl_busy) {
		set_status("Busy - try again in a moment");
		return;
	}
	if (!src_supports_type(type_sel)) {
		if (src_sel == 1) {
			set_status("ambientCG has no model search - use Poly Haven for models");
		}
		else if (src_sel == 4) {
			set_status("Blendkit materials are .blend only - use Models or HDRIs");
		}
		else {
			set_status("This source only has models");
		}
		return;
	}
	if (src_sel == 0) {
		char *cached = ph_json_tex;
		if (type_sel == 1) {
			cached = ph_json_mdl;
		}
		if (type_sel == 2) {
			cached = ph_json_hdr;
		}
		if (!string_equals(cached, "")) {
			ph_json = cached;
			apply_ph_results();
			return;
		}
		char *t = "textures";
		if (type_sel == 1) {
			t = "models";
		}
		if (type_sel == 2) {
			t = "hdris";
		}
		op = 1;
		set_status("Fetching Poly Haven catalog...");
		script_file_download(string("https://api.polyhaven.com/assets?t=%s", t), NULL, on_search_json, 0, NULL);
	}
	else if (src_sel == 1) {
		char *t = "Material";
		if (type_sel == 2) {
			t = "HDRI";
		}
		char *q = string_replace_all(search, " ", "%20");
		q = string_replace_all(q, "&", "%26");
		op = 1;
		set_status("Searching ambientCG...");
		script_file_download(string("https://ambientcg.com/api/v2/full_json?type=%s&q=%s&limit=10&offset=%d&sort=popular&include=previewData", t, q, page * 10), NULL,
		                     on_search_json, 0, NULL);
	}
	else if (src_sel == 2) {
		if (string_equals(POLY_PIZZA_KEY, "")) {
			set_status("Set POLY_PIZZA_KEY in data/.env - free key at poly.pizza/settings/api");
			st_marker();
			return;
		}
		char *q   = urlencode(search);
		char *cur = "";
		if (page_curs != NULL) {
			if (page < page_curs->length) {
				cur = page_curs->buffer[page];
			}
		}
		char *u = string("https://api.poly.pizza/v1/search/%s?limit=10&format=glb", q);
		if (string_equals(q, "")) {
			u = string("https://api.poly.pizza/v1/popular?limit=10&format=glb");
		}
		if (!string_equals(cur, "")) {
			u = string("%s&cursor=%s", u, cur);
		}
		op = 1;
		set_status("Searching poly.pizza...");
		script_file_download(u, string("X-Auth-Token: %s\r\n", POLY_PIZZA_KEY), on_search_json, 0, NULL);
	}
	else if (src_sel == 3) {
		char *lic = "cc0";
		if (qual_sel == 1) {
			lic = "by";
		}
		if (qual_sel == 2) {
			lic = "by-sa";
		}
		char *q   = urlencode(search);
		char *cur = "";
		if (page_curs != NULL) {
			if (page < page_curs->length) {
				cur = page_curs->buffer[page];
			}
		}
		char *u = string("https://api.sketchfab.com/v3/search?q=%s&type=models&downloadable=true&license=%s&count=10", q, lic);
		if (!string_equals(cur, "")) {
			u = string("%s&cursor=%s", u, cur);
		}
		char *h = NULL;
		if (!string_equals(SKETCHFAB_TOKEN, "")) {
			h = string("Authorization: Token %s\r\n", SKETCHFAB_TOKEN);
		}
		op = 1;
		set_status("Searching Sketchfab...");
		script_file_download(u, h, on_search_json, 0, NULL);
	}
	else {
		char *t = "model";
		if (type_sel == 2) {
			t = "hdr";
		}
		char *q = urlencode(search);
		op = 1;
		set_status("Searching Blendkit...");
		script_file_download(string("https://www.blendkit.com/api/v1/search/?query=%s+asset_type:%s+is_free:true&dict_parameters=1&page_size=10&page=%d", q, t, page + 1),
		                     NULL, on_search_json, 0, NULL);
	}
}

void do_search() {
	page = 0;
	if (src_sel == 2 || src_sel == 3) {
		page_curs = string_array_create(1);
		page_curs->buffer[0] = string("");
	}
	fetch_results();
}

// ---- UI ----

void on_ui() {
	if (!ui_panel(&expanded, "Free Library", false, false, false)) {
		return;
	}
	if (first_ui) {
		first_ui = 0;
		do_search();
	}

	ui_row(row3);
	ui_combo(&src_sel, src_items, "Source", true, UI_ALIGN_LEFT, true);
	ui_combo(&type_sel, type_items, "Type", true, UI_ALIGN_LEFT, false);
	if (src_sel == 3) {
		ui_combo(&qual_sel, sf_lic_items, "Licence", true, UI_ALIGN_LEFT, false);
	}
	else {
		ui_combo(&qual_sel, qual_items, "Quality", true, UI_ALIGN_LEFT, false);
	}

	ui_row(row_search);
	ui_text_input(&search, "Search", UI_ALIGN_LEFT, true, true);
	if (ui_button("Search", UI_ALIGN_CENTER, "")) {
		if (!dl_busy && op == 0) {
			do_search();
		}
		else {
			set_status("Busy - try again in a moment");
		}
	}

	if (have_results) {
		for (int i = 0; i < res_disp->length; i++) {
			ui_row(row_res);
			char *thumb = res_thumbs->buffer[i];
			if (!string_equals(thumb, "") && iron_file_exists(thumb) && iron_file_size(thumb) > 0) {
				void *tex = data_get_texture(thumb);
				if (tex != NULL) {
					ui_image(tex, 0xffffffff, 44);
				}
				else {
					ui_text("", 0, 0);
				}
			}
			else {
				ui_text("", 0, 0);
			}
			ui_text(res_disp->buffer[i], 0, 0);
			if (ui_button("Get", UI_ALIGN_CENTER, "")) {
				if (dl_busy) {
					set_status("Already downloading - please wait");
				}
				else if (op != 0 && op != 4) {
					set_status("Busy - try again in a moment");
				}
				else {
					start_download(res_page_ids->buffer[i]);
				}
			}
		}

		ui_row(row3);
		if (ui_button("Prev", UI_ALIGN_CENTER, "")) {
			if (page > 0) {
				page = page - 1;
				fetch_results();
			}
		}
		ui_text(page_label, 0, 0);
		if (ui_button("Next", UI_ALIGN_CENTER, "")) {
			int can_next = 1;
			if (src_sel == 0 && page + 1 >= page_count) {
				can_next = 0;
			}
			if (src_sel == 2 || src_sel == 3) {
				if (page_curs == NULL) {
					can_next = 0;
				}
				else {
					if (page + 1 >= page_curs->length) {
						can_next = 0;
					}
				}
			}
			if (can_next) {
				page = page + 1;
				fetch_results();
			}
		}
	}

	ui_row(row3);
	if (ui_button("Poly Haven", UI_ALIGN_CENTER, "")) {
		file_start("https://polyhaven.com");
	}
	if (ui_button("ambientCG", UI_ALIGN_CENTER, "")) {
		file_start("https://ambientcg.com");
	}
	if (ui_button("3D Assets", UI_ALIGN_CENTER, "")) {
		file_start("https://3dassets.one");
	}

	ui_row(row3);
	if (ui_button("poly.pizza", UI_ALIGN_CENTER, "")) {
		file_start("https://poly.pizza");
	}
	if (ui_button("Sketchfab", UI_ALIGN_CENTER, "")) {
		file_start("https://sketchfab.com");
	}
	if (ui_button("Blendkit", UI_ALIGN_CENTER, "")) {
		file_start("https://www.blendkit.com");
	}

	ui_text(status, 0, 0);
}

// ---- per-frame pump ----

void on_update() {
	if (st_pending) {
		// rewritten every frame: the test driver's startup delete may land
		// after the first write, this heals it on the next frame
		iron_file_save_bytes("/tmp/opencode/fl_done", sys_string_to_buffer("done"), 4);
	}
	// clamp type to what the current source offers
	if ((src_sel == 2 || src_sel == 3) && type_sel != 1) {
		type_sel = 1;
	}
	if (src_sel == 4 && type_sel == 0) {
		type_sel = 1;
	}
	if (src_sel != live_src || type_sel != live_type) {
		live_src  = src_sel;
		live_type = type_sel;
		if (have_results) {
			have_results = 0;
			page = 0;
			set_status("Source/Type changed - press Search");
		}
	}

	if (dl_kick) {
		dl_kick = 0;
		dl_i = dl_i + 1;
		next_dl();
	}
	else {
		pump_thumbs();
	}

	if (st_kick) {
		st_kick = 0;
		if (res_ids != NULL && res_ids->length > 0) {
			start_download(res_ids->buffer[0]);
		}
		else {
			set_status("SELFTEST: no results");
			console_info("SELFTEST NO RESULTS");
			st_marker();
		}
	}
}

// ---- .env loading (keys + paths, see .env.example) ----

char *env_trim(char *s) {
	int len = string_length(s);
	int a = 0;
	int b = len;
	int c = 0;
	while (a < len) {
		c = char_code_at(s, a);
		if (c == 32 || c == 9 || c == 13 || c == 34 || c == 39) {
			a = a + 1;
		}
		else {
			break;
		}
	}
	while (b > a) {
		c = char_code_at(s, b - 1);
		if (c == 32 || c == 9 || c == 13 || c == 34 || c == 39) {
			b = b - 1;
		}
		else {
			break;
		}
	}
	if (b <= a) {
		return substring(s, 0, 0);
	}
	return substring(s, a, b);
}

// return value of "key=" in an .env body, "" when absent
char *env_get(char *content, char *key) {
	char *needle = string("%s=", key);
	int klen = string_length(needle);
	int clen = string_length(content);
	int off = 0;
	int nl = 0;
	char *line = "";
	while (off < clen) {
		nl = off;
		while (nl < clen) {
			if (char_code_at(content, nl) == 10) {
				break;
			}
			nl = nl + 1;
		}
		line = env_trim(substring(content, off, nl));
		if (str_starts(line, needle)) {
			return env_trim(substring(line, klen, string_length(line)));
		}
		off = nl + 1;
	}
	return substring(content, 0, 0);
}

void load_env() {
	buffer_t *b = data_get_blob(".env");
	if (b == NULL) {
		b = data_get_blob("./.env");
	}
	if (b == NULL) {
		return;
	}
	char *content = sys_buffer_to_string(b);
	char *v = substring(content, 0, 0);
	v = env_get(content, "LIB_ROOT");
	if (!string_equals(v, "")) {
		LIB_ROOT = v;
	}
	v = env_get(content, "THUMB_DIR");
	if (!string_equals(v, "")) {
		THUMB_DIR = v;
	}
	v = env_get(content, "POLY_PIZZA_KEY");
	if (!string_equals(v, "")) {
		POLY_PIZZA_KEY = v;
	}
	v = env_get(content, "SKETCHFAB_TOKEN");
	if (!string_equals(v, "")) {
		SKETCHFAB_TOKEN = v;
	}
	console_info("Free Library: .env loaded");
}

void main() {
	load_env();
	plugin = plugin_create();
	plugin_notify_on_ui(plugin, on_ui);
	plugin_notify_on_update(plugin, on_update);

	src_items = string_array_create(5);
	src_items->buffer[0] = "Poly Haven";
	src_items->buffer[1] = "ambientCG";
	src_items->buffer[2] = "poly.pizza";
	src_items->buffer[3] = "Sketchfab";
	src_items->buffer[4] = "Blendkit";
	type_items = string_array_create(3);
	type_items->buffer[0] = "Materials";
	type_items->buffer[1] = "Models";
	type_items->buffer[2] = "HDRIs";
	qual_items = string_array_create(3);
	qual_items->buffer[0] = "1K";
	qual_items->buffer[1] = "2K";
	qual_items->buffer[2] = "4K";
	sf_lic_items = string_array_create(3);
	sf_lic_items->buffer[0] = "CC0";
	sf_lic_items->buffer[1] = "Attribution";
	sf_lic_items->buffer[2] = "ShareAlike";

	row3 = f32_array_create(3);
	row3->buffer[0] = 0.34;
	row3->buffer[1] = 0.33;
	row3->buffer[2] = 0.33;
	row_search = f32_array_create(2);
	row_search->buffer[0] = 0.75;
	row_search->buffer[1] = 0.25;
	row_res = f32_array_create(3);
	row_res->buffer[0] = 0.14;
	row_res->buffer[1] = 0.56;
	row_res->buffer[2] = 0.30;

	iron_create_directory(THUMB_DIR);
	iron_create_directory(LIB_ROOT);

	if (selftest) {
		// 1=PH material 2=acg material 3=PH HDRI 4=PH model
		// 5=poly.pizza model 6=Sketchfab model 7=Blendkit model 8=Blendkit HDRI
		console_info("SELFTEST START");
		page = 0;
		src_sel  = 0;
		type_sel = 0;
		search   = "wood";
		if (selftest == 2) {
			src_sel = 1;
		}
		if (selftest == 3) {
			type_sel = 2;
			search   = "sky";
		}
		if (selftest == 4) {
			type_sel = 1;
			search   = "chair";
		}
		if (selftest == 5) {
			src_sel  = 2;
			type_sel = 1;
			search   = "chair";
		}
		if (selftest == 6) {
			src_sel  = 3;
			type_sel = 1;
			search   = "chair";
			qual_sel = 0;
		}
		if (selftest == 7) {
			src_sel  = 4;
			type_sel = 1;
			search   = "chair";
		}
		if (selftest == 8) {
			src_sel  = 4;
			type_sel = 2;
			search   = "sky";
			qual_sel = 1;
		}
		do_search();
	}
}
