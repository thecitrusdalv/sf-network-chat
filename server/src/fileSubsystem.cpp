#include "../headers/fileSubsystem.h"
#include "../headers/server.h"

bool FileSubsystem::init()
{
	std::cout << FS_LOG_MESSAGE << "initialization" << std::endl;
	//Попытка открыть(создать) файл глобальных сообщений
	if(!open_file(globalMessagesFile, GLOBAL_MESSAGES_FILE))
		return false;

	//Попытка открыть(создать) файл пользователей
	if (!open_file(usersFile, USERS_FILE))
		return false;

	std::cout << FS_LOG_MESSAGE << "initialization complete" << std::endl;
	return true;
}

bool FileSubsystem::bindUserPersonalFile (const std::string& login, std::fstream& file)
{
	//Попытка открыть(создать) файл персональных сообщений пользователя
	if (!open_file(file, PERSONAL_MESSAGES_PATH + login))
		return false;

	return true;
}

/*
 * Функция open_file принимает файловый поток и связывает его с файлом, переданным вторым аргументом.
 * Если имя файла содержит в себе директории, создается string_view с именем нужной директории
 * путем отсекания названия файла с конца до первого символа '/'.
 *
 * Если строка с директорией не пуста, значит в имени файла присутствует пусть с директорями, 
 * и их нужно создать.
 *
 * При создании директории (если ее небыло) происходит проверка на ошибку создания.
 * Идентификатор ошибки кладется в переменную err. Далее выводится причина, если была ошибка и 
 * функция возвращает false.
 *
 * После создания директории, происходит открытие(создание) необходимого файла.
 * При этом функция пытается выставить права на открываемый(создаваемый) файл каждый раз 
 * через функцию set_permissions().
*/

bool FileSubsystem::open_file(std::fstream& file, const string& filename)
{
	//Создаем string_view от имени файла. Отсекаем его до первого символа '/'
	//Либо полностью если нет пути в имени файла
	
	std::string_view parrent_path {filename};

	for (size_t i = filename.size()-1; i < filename.size(); i--) {
		if (filename[i] == '/')
			break;
		parrent_path.remove_suffix(1);
	}

	//Если есть директория
	if (parrent_path.size()) {

		//Попытка создать ее
		std::error_code err;
		fs::create_directories(parrent_path, err);

		if (err.value()) {
			std::cerr << FS_LOG_MESSAGE << err.message() << std::endl;
			std::cerr << FS_LOG_MESSAGE << "cannot create parrent directory '" << parrent_path << "' for relations files" << std::endl;
			return false;
		}
	}

	//Попытка открыть файл либо создать с выставлением прав на него
	file.open(filename, std::ios_base::in | std::ios_base::out);

	if (file)
		set_permissions(filename);
	else {
			file.open(filename, std::ios_base::in | std::ios_base::out | std::ios_base::trunc);

			if (file) {

				std::cout << FS_LOG_MESSAGE << filename << ": file not found, new file exist." << std::endl;
				set_permissions(filename);
				return true;

			} else {

				std::cerr << FS_LOG_MESSAGE << filename << ": " << std::strerror(errno) << std::endl;
				std::cerr << FS_LOG_MESSAGE << "file not opened" << std::endl;
				return false;

			}
	}

	return true;
}

//Принимает имя файла, выставляет права на него
void FileSubsystem::set_permissions(const std::string& filename)
{
			fs::permissions(filename, fs::perms::group_all | fs::perms::others_all, fs::perm_options::remove);
			fs::permissions(filename, fs::perms::owner_write | fs::perms::owner_read, fs::perm_options::add);
}
