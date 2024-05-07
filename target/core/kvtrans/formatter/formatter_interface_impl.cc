#include "formatter_interface.h"
#include "formatter.h"
#include "parser.h"

void dss_do_format(Parser::ParsedPayloadSharedPtr payload)
{
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
}

void formatter_run_cmdline(dss_formatter_config_opts_t *opts)
{
    // Allocate memory for ParsedPayload
    Parser::ParsedPayloadSharedPtr payload =
        std::make_shared<Parser::ParsedPayload>();

    payload->block_allocator_type = opts->ba_type;
    payload->device_name = opts->dev_name;
    payload->is_debug = opts->debug;
    payload->logical_block_size = opts->blk_size;
    payload->num_block_states = opts->nblk_states;
    payload->status = true;

    dss_do_format(payload);

    return;
}

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

    dss_do_format(payload);

    std::cout<<"Formatter Init (open and format) completed"<<std::endl;

    return;
}
