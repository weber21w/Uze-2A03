
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

FILE *dbf;
FILE *dbft;//hacky, write a temporary file before writing the directory...

int directory_index = 0;
int vgm_cursor_pos = 0;
unsigned char directory_data[512];

int parse_ext(const struct dirent *dir){
	if(!dir)
		return 0;

	if(dir->d_type == DT_REG){ /* only deal with regular file */
		const char *ext = strrchr(dir->d_name,'.');
		if((!ext) || (ext == dir->d_name))
			return 0;
		else{
			if(strcmp(ext, ".vgm") == 0)
				return 1;
		}
	}

	return 0;
}

int AppendFile(char *fname){
	FILE *fi = fopen(fname, "rb");
	if(fi == NULL)
		return -1;

	directory_data[(directory_index*2)+0] = ((vgm_cursor_pos/512)&0xFF00)>>8;//records start of .vgm file data(sector offset)
	directory_data[(directory_index*2)+1] = ((vgm_cursor_pos/512)&0x00FF)>>0;

	directory_index++;

	//fake image data for now...
	int i;
	for(i=0;i<8*5*64;i++){
		fputc(0, dbft);
		vgm_cursor_pos++;
	}
	
	while(!feof(fi)){
		fputc(fgetc(fi), dbft);
		vgm_cursor_pos++;
	}
	while(vgm_cursor_pos%512){//pad out to sector boundary
		fputc(0, dbft);
		vgm_cursor_pos++;
	}

	fclose(fi);

	return 0;
}

int FinalizeFile(){//hack, use the directory and the temporary file to create the final file that should have been done in RAM...
	fclose(dbft);
	dbft = fopen("tmp.dat","rb");
	if(dbft == NULL)
		return -1;
	dbf = fopen("../default/VGMNES01.DAT","wb");
	if(dbft == NULL)
		return -1;

	int i;
	for(i=0;i<512;i++)
		fputc(directory_data[i], dbf);

	while(!feof(dbft))
		fputc(fgetc(dbft), dbf);

	fclose(dbft);
	fclose(dbf);
	return 0;
}

int main(int argc, char *argv[]){
	char search_dir[1024];
	char search_file[2048];
	struct dirent **namelist;
	int file_count = 0;
	int i;

	if(argc > 1){
		strncpy(search_dir,argv[1],sizeof(search_dir)-3);
	}else
		strcpy(search_dir,"vgm");

	dbft = fopen("tmp.dat","wb");
	if(dbft == NULL){
		printf("ERROR: failed to create temporary file\n");
		return -1;
	}

	for(i=0;i<sizeof(directory_data);i++)
		directory_data[i] = 0xFF;

	file_count = scandir(search_dir, &namelist, parse_ext, alphasort);
	if(file_count < 0){
		printf("ERROR: failed to open [%s]\n", search_dir);
		return 1;
	}else{
	
		for(i=0;i<file_count;i++){
			snprintf(search_file, sizeof(search_file)-3, "%s/%s", search_dir, namelist[i]->d_name);
			free(namelist[i]);
			printf("Appending: [%s]\n", search_file);
			if(AppendFile(search_file) < 0){
				printf("ERROR: failed to parse [%s]\n", search_file);
				free(namelist);
				fclose(dbft);
				fclose(dbf);
				return -1;
			}
		}
		free(namelist);
	}

	FinalizeFile();
	return 0;
}
