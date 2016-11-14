#pragma once

class ClonedProject;
class TokenizedFile;

class Db {
public:
    static exec(std::string const & query);


    static void addProject(ClonedProject * project) {

    }

    static void addFile(TokenizedFile * file) {

    }
};
