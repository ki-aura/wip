#include "gex.h"

bool helperfunction_open_file(void){
	// try to open it
	app.fd = open(app.fname, O_RDWR);
	if (app.fd < 0) {
		return false;
	}

	// get file stats
	if (fstat(app.fd, &app.fs) < 0) {
		close(app.fd);
		return false;
	}

	// check file not empty
	if (app.fs.st_size < 1) {
		close(app.fd);
		return false;	
	} else {
		app.fsize = app.fs.st_size;
	}

	app.map = mmap(NULL, app.fsize, PROT_READ | PROT_WRITE, MAP_SHARED, app.fd, 0);
	if (app.map == MAP_FAILED) {
		close(app.fd);
		return false;	
	}

	return true;
}

bool open_file(int argc, char *argv[])
{
	bool valid_file = true;
	// check we have something passed on the command line
	if (argc != 2) valid_file = false;
	// check for help; version handled below
	if (valid_file) 
		if (strcmp( argv[1],"--help")==0 || strcmp( argv[1],"-h")==0 ) valid_file = false;
	
	if (!valid_file){
		putp(tigetstr("rmcup"));
		endwin();
		fputs(	"Usage:\n"
				"  gex <file name>         edit file\n"
				"  gex -v or --version     shows current version\n"
				"  gex -h or --help        displays this message\n", stderr);
		final_close(0);
		return false;
	} else {
		if (strcmp(argv[1],"--version")==0 || strcmp(argv[1],"-v")==0 ) {
			putp(tigetstr("rmcup"));
			endwin();
            snprintf(tmp, 40, "Version %s\n", GEX_VERSION);
			fputs(tmp, stderr);
			final_close(0);
			return false;
		} else {
			app.fname = argv[1];
			return helperfunction_open_file();
		}
	}
}

void close_file(void)
{
	munmap(app.map, app.fsize);
	close(app.fd);
}

// Comparison for qsort: by int key
typedef struct {
	unsigned long key;
	unsigned char val; 
} kv_t;

int cmp_key(const void *a, const void *b) {
    const kv_t *pa = (const kv_t*)a;
    const kv_t *pb = (const kv_t*)b;
    if (pa->key < pb->key) return -1;
    if (pa->key > pb->key) return 1;
    return 0;
}
 
void save_changes(void){
	if (kh_size(app.edmap) == 0)
		popup_question("No changes made",
			"Press any key to continue", PTYPE_CONTINUE);
	else if(popup_question("Are you sure you want to save changes?",
			"This action can not be undone (y/n)", PTYPE_YN)){
	
		// Allocate array to hold all kv pairs
		size_t n = kh_size(app.edmap);
		kv_t *arr = malloc(sizeof(kv_t) * n);
	
		// Copy hash table entries into array
		size_t idx = 0;
		khint_t k;
		kh_foreach(app.edmap, k) {
			if (kh_exist(app.edmap, k)) {
				arr[idx].key = kh_key(app.edmap, k);
				arr[idx].val = kh_val(app.edmap, k);
				idx++;
			}
		}
	
		// Sort array by key
		qsort(arr, n, sizeof(kv_t), cmp_key);
	
		// Apply changes in sorted order
		for (size_t i = 0; i < n; i++) {
			app.map[arr[i].key] = arr[i].val;
		}

		// and sync it out
		msync(app.map, app.fsize, MS_SYNC);
		// clear change history as these are now permanent
		kh_clear(app.edmap);
        free(arr);
        
		// refresh to get rid of old change highlights
		update_all_windows();
		handle_global_keys(KEY_REFRESH);
	}	
}

void abandon_changes(void){
    if (kh_size(app.edmap) == 0)
        popup_question("No changes to abandon",
            "Press any key to continue", PTYPE_CONTINUE);
    else if(popup_question("Are you sure you want to abandon changes?",
            "This action can not be undone (y/n)", PTYPE_YN)){
    
        // abandon changes
        kh_clear(app.edmap);
        // refresh to get rid of old change highlights
        update_all_windows();
        handle_global_keys(KEY_REFRESH);
    }
}


// helper: build temp filename "<fname>.gex"
static char *make_temp_name(const char *fname) {
    size_t len = strlen(fname) + 8;
    char *tmpnam = malloc(len);
    if (!tmpnam) return NULL;
    snprintf(tmpnam, len, "%s.gextmp", fname);
    return tmpnam;
}

// portable copy from fd src to fd dst for count bytes
static int copy_bytes(int dst, int src, off_t count) {
	int COPY_BUF_SIZE=65536;
    char buf[COPY_BUF_SIZE];
    while (count > 0) {
        ssize_t to_read = count < COPY_BUF_SIZE ? count : COPY_BUF_SIZE;
        ssize_t n = read(src, buf, to_read);
        if (n <= 0) return -1; // error or unexpected EOF
        if (write(dst, buf, n) != n) return -1;
        count -= n;
    }
    return 0;
}

// Insert nbytes of zeros after f_offset
int file_insert(off_t f_offset, size_t nbytes) {
    int ret = -1;
    char *tmpname = make_temp_name(app.fname);
    if (!tmpname) return -1;

    // unmap original file
    if (app.map) {
        munmap(app.map, app.fsize);
        app.map = NULL;
    }

    int tfd = open(tmpname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tfd < 0) goto out_free;

    // copy head
    if (lseek(app.fd, 0, SEEK_SET) < 0) goto out_close;
    if (f_offset > 0) {
        if (copy_bytes(tfd, app.fd, f_offset) < 0) goto out_close;
    }

    // write inserted bytes
    char *zeros = calloc(1, nbytes);
    if (!zeros) goto out_close;
    if (write(tfd, zeros, nbytes) != (ssize_t)nbytes) { free(zeros); goto out_close; }
    free(zeros);

    // copy tail
    off_t tail = app.fsize - f_offset;
    if (tail > 0) {
        if (copy_bytes(tfd, app.fd, tail) < 0) goto out_close;
    }

    ret = 0;

out_close:
    close(tfd);
    if (ret == 0) {
        close(app.fd);
        if (rename(tmpname, app.fname) == 0) {
            app.fd = open(app.fname, O_RDWR);
            if (app.fd >= 0) {
                struct stat st;
                if (fstat(app.fd, &st) == 0)
                    app.fsize = st.st_size;
                else ret = -1;
            } else ret = -1;
        } else ret = -1;
    } else {
        unlink(tmpname);
    }

out_free:
    free(tmpname);
    return ret;
}

// Delete nbytes after f_offset
int file_delete(off_t f_offset, size_t nbytes) {
    int ret = -1;
    char *tmpname = make_temp_name(app.fname);
    if (!tmpname) return -1;

    // unmap original file
    if (app.map) {
        munmap(app.map, app.fsize);
        app.map = NULL;
    }

    int tfd = open(tmpname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tfd < 0) goto out_free;

    // copy head
    if (lseek(app.fd, 0, SEEK_SET) < 0) goto out_close;
    if (f_offset > 0) {
        if (copy_bytes(tfd, app.fd, f_offset) < 0) goto out_close;
    }

    // skip nbytes in source
    if (lseek(app.fd, f_offset + nbytes, SEEK_SET) < 0) goto out_close;

    // copy tail
    off_t tail = app.fsize - (f_offset + nbytes);
    if (tail > 0) {
        if (copy_bytes(tfd, app.fd, tail) < 0) goto out_close;
    }

    ret = 0;

out_close:
    close(tfd);
    if (ret == 0) {
        close(app.fd);
        if (rename(tmpname, app.fname) == 0) {
            app.fd = open(app.fname, O_RDWR);
            if (app.fd >= 0) {
                struct stat st;
                if (fstat(app.fd, &st) == 0)
                    app.fsize = st.st_size;
                else ret = -1;
            } else ret = -1;
        } else ret = -1;
    } else {
        unlink(tmpname);
    }

out_free:
    free(tmpname);
    return ret;
}



void insert_bytes(void){
unsigned long byteins, ins_offset;
    if (kh_size(app.edmap) > 0)
        popup_question("Save changes before inserting bytes",
            "Press any key to continue", PTYPE_CONTINUE);
    else {
    	ins_offset = cursor_full_file_offset();
    	// tell where insert will be, get number of bytes to insert, warn 
    	snprintf(tmp, 250, "How Many Bytes to INSERT AT offset %lu? (max 1024)",ins_offset) ;
        // hex.v_start = will either be a new valid value or 0
        byteins = popup_question(tmp, "", PTYPE_UNSIGNED_LONG);
		if(byteins>1024) byteins=1024;
		if(byteins<=0) return;
        
    	snprintf(tmp, 250, "Confirm: Insert %lu Bytes?",byteins) ;        
        if( popup_question(tmp, "This Action Can NOT Be Undone (y/n)", PTYPE_YN)) {
			// close file, create new file, rename, open new file
			file_insert(ins_offset, byteins);
			helperfunction_open_file();
			// and refresh
			create_windows();
        }
    }
}

void delete_bytes(void){
unsigned long bytedel, del_offset, max_pos;
    if (kh_size(app.edmap) > 0)
        popup_question("Save changes before deleting bytes",
            "Press any key to continue", PTYPE_CONTINUE);
    else {
    	// tell where insert will be, get number of bytes to insert,
    	del_offset = cursor_full_file_offset();
		// check we're not trying to delete past end of file
    	max_pos = app.fsize - del_offset;
    	max_pos = max_pos <= 1024 ? max_pos : 1024;
    	
		// find how many bytes to delete
    	snprintf(tmp, 250, "How Many Bytes to DELETE FROM offset %lu? (max %d)", del_offset, (int)max_pos);
        bytedel = popup_question(tmp, "", PTYPE_UNSIGNED_LONG);
        // sense checks
		if(bytedel>max_pos) bytedel=max_pos;
		if(bytedel<=0) return;
		
    	snprintf(tmp, 250, "Confirm: Delete %lu Bytes?",bytedel) ;        
        if( popup_question(tmp, "This Action Can NOT Be Undone (y/n)", PTYPE_YN)) {
			// close file, create new file, rename, open new file
			file_delete(del_offset, bytedel);
			
			// We only want to open the file if it's still > 1 byte; otherwise exit 
			if (helperfunction_open_file()) 			
				create_windows();
			else 
				final_close(0);
    	}
    }
}




