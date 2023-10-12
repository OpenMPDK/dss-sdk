#include "formatter_interface.h"
#include "formatter.h"

void formatter_init(formatter_conf_t *conf) {

    int argc = conf->argc;
    char **argv = conf->argv;

    if (argc < 3) {
        printf("usage: %s <conf-file> <conf-filetype>\n", argv[0]);
        assert(("ERROR", false));
        exit(1);
    }

    std::string conf_file = argv[1];
    std::string conf_type = argv[2];
    Parser::ParserType parser_type = Parser::ParserType::TEXT;
    Parser::ParserInterfaceSharedPtr parser = nullptr;
    Parser::ParsedPayloadSharedPtr payload = nullptr;

    if (conf_type.compare("TEXT") == 0) {
        //Parser::ParserType p = Parser::ParserType::TEXT;
        // Default parser type is text
        // Create a text parser
        parser = std::make_shared<Parser::TextParser>();
        if (parser == nullptr) {
            // Log OOM
            assert(("ERROR", false));
        }
    }

    // Parse the payload accordingly
    payload = parser->parse_payload(parser_type, conf_file);
    // Check parse status before using parsed object
    if (!payload->status) {
        assert(("ERROR, false"));
    }

    // Initialize a formatter instance
    Format::FormatterSharedPtr f = std::make_shared<Format::Formatter>();

    // Open device and build formatter state
    if (!f->open_device(payload)) {
        assert(("ERROR", false));
    }

    // Format the device
    if (!f->format_device(payload->is_debug)) {
        assert(("ERROR", false));
    }

    std::cout<<"Formatter Init (open and format) completed"<<std::endl;

    return;
}
