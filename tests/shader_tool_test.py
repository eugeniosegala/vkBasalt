import json, os, pathlib, subprocess, sys, tempfile
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    shader = root / 'controls.FX'
    (root / 'options.fxh').write_text('''
uniform float2 Tint < ui_label = "Tint colour"; ui_min = 0.0; ui_max = 1.0; > = float2(0.2, 0.8);
uniform int Mode < ui_type = "combo"; ui_items = "First\0Second\0"; > = 1;
uniform bool Enabled = true;
uniform float Zero;
uniform float Hidden < hidden = true; > = 1.0;
uniform float Time < source = "timer"; >;
''')
    shader.write_text('''#include "options.fxh"
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return float4(Tint, Mode + Zero, Enabled); }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
''')
    config = root / 'shader.conf'
    config.write_text(f'effects = A:B:makoDeband\nA = "{shader}"\nB = "{shader}"\n')
    def read():
        return json.loads(subprocess.check_output([sys.argv[1], str(config)], env={**os.environ,'VKBASALT_LOG_LEVEL':'none'}))['effects']
    first = read()
    assert len(first) == 3, first
    for effect in first: assert effect['error'] is None, effect
    parameters = first[0]['parameters']
    assert len(parameters) == 5, parameters
    assert parameters[0]['label'] == 'Tint colour [1]' and parameters[1]['label'] == 'Tint colour [2]'
    assert parameters[2]['items'] == ['First','Second']
    assert parameters[4]['default'] == 0
    assert len(first[2]['parameters']) == 5
    with config.open('a') as f: f.write(parameters[0]['key'] + ' = 0.6\n')
    changed = read()
    assert abs(changed[0]['parameters'][0]['value'] - .6) < 1e-6
    assert changed[0]['parameters'][1]['value'] == first[0]['parameters'][1]['value']
    assert changed[1]['parameters'] == first[1]['parameters']
    with config.open('a') as f: f.write(parameters[0]['key'] + ' = default\n')
    reset = read()
    assert reset[0]['parameters'][0]['value'] == first[0]['parameters'][0]['default']
    shader.write_text('invalid source')
    failed = read()
    assert failed[0]['error'] and failed[1]['error'] and not failed[2]['error']

    # Scalar arrays used to interpret literal values as component IDs and abort.
    # Cover initialized/zero scalar arrays alongside vector arrays and matrices.
    for declaration, expression, defaults in (
        ('uniform float Weights[2] = {0.25, 0.75};', 'Weights[0]', [0.25, 0.75]),
        ('uniform float Weights[2];', 'Weights[0]', [0, 0]),
        ('uniform float Weights[2] = {0.25, 0.75};', 'Weights[(uint)p.x % 2]', [0.25, 0.75]),
        ('uniform float Weights[2];', 'Weights[(uint)p.x % 2]', [0, 0]),
        ('uniform bool Weights[2] = {true, false};', 'Weights[(uint)p.x % 2] ? 1.0 : 0.0', [1, 0]),
        ('uniform int Weights[2] = {1, 2};', 'Weights[(uint)p.x % 2]', [1, 2]),
        ('uniform uint Weights[2] = {1, 2};', 'Weights[(uint)p.x % 2]', [1, 2]),
        ('uniform float2 Weights[2];', 'Weights[(uint)p.x % 2].x', [0, 0, 0, 0]),
        ('uniform float2 Weights[2] = {float2(0.25, 0.75), float2(0.5, 1)};', 'Weights[0].x', [0.25, 0.75, 0.5, 1]),
        ('uniform float2x2 Weights = float2x2(0.25, 0.75, 0.5, 1);', 'Weights[0][0]', [0.25, 0.75, 0.5, 1]),
    ):
        shader.write_text(declaration + """
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return """ + expression + """; }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
""")
        config.write_text(f'effects = A\nA = "{shader}"\n')
        effect = read()[0]
        assert effect['error'] is None, effect
        assert [p['default'] for p in effect['parameters']] == defaults, effect
        keys = [p['key'] for p in effect['parameters']]
        assert len(set(keys)) == len(defaults), effect
        value = 0.125 if effect['parameters'][-1]['type'] == 'float' else 1
        with config.open('a') as f: f.write(keys[-1] + f' = {value}\n')
        assert [p['value'] for p in read()[0]['parameters']] == defaults[:-1] + [value]

    # Presentation is opt-in and uses original compiler dimensions, not names
    # or flattened component counts. Empty enum labels must preserve indices.
    for declaration, expression, count, widget in (
        ('uniform float3 Colour < ui_type = "color"; ui_label = "Border colour"; > = float3(0.1, 0.2, 0.3);', 'Colour.x', 3, 'color'),
        ('uniform float4 Colour < ui_type = "color"; > = float4(0.1, 0.2, 0.3, 0.4);', 'Colour.x', 4, 'color'),
        ('uniform float3 Colour;', 'Colour.x', 3, ''),
        ('uniform float2 Colour < ui_type = "color"; >;', 'Colour.x', 2, ''),
        ('uniform int3 Colour < ui_type = "color"; >;', 'Colour.x', 3, ''),
        ('uniform float Colour < ui_type = "color"; >;', 'Colour', 1, ''),
        ('uniform float Colour[3] < ui_type = "color"; >;', 'Colour[0]', 1, ''),
        ('uniform float2 Colour[2] < ui_type = "color"; >;', 'Colour[0].x', 1, ''),
        ('uniform float2x2 Colour < ui_type = "color"; >;', 'Colour[0][0]', 1, ''),
        ('uniform float3 Colour < ui_type = "future-widget"; >;', 'Colour.x', 3, ''),
        ('uniform float3 Colour < ui_type = 2; >;', 'Colour.x', 3, ''),
        ('uniform float Colour < ui_type = "input"; ui_min = 0.0; ui_max = 1.0; >;', 'Colour', 1, 'input'),
        ('uniform int Colour < ui_type = "combo"; ui_items = "First\0\0Third\0"; > = 2;', 'Colour', 1, 'combo'),
        ('uniform uint Colour < ui_type = "radio"; ui_items = "First\0\0Third\0"; > = 2;', 'Colour', 1, 'radio'),
        ('uniform float Colour < ui_type = "combo"; ui_items = "First\0Second\0"; >;', 'Colour', 1, ''),
        ('uniform int2 Colour < ui_type = "combo"; ui_items = "First\0Second\0"; >;', 'Colour.x', 2, ''),
    ):
        shader.write_text(declaration + """
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return """ + expression + """; }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
""")
        config.write_text(f'effects = A\nA = "{shader}"\n')
        effect = read()[0]
        assert effect['error'] is None, effect
        parameters = effect['parameters']
        for index, p in enumerate(parameters):
            hint = p['presentation']
            assert hint['count'] == count and hint['widget'] == widget, (declaration, p)
            assert hint['component'] == (index if count > 1 else 0), p
            assert hint['group'] == (parameters[0]['key'] if count > 1 else p['key']), p
            assert p['key'] == f'makoShader_41_436f6c6f7572_{index}', p
            if widget in ('combo', 'radio'):
                assert p['items'] == ['First', '', 'Third'] and p['value'] == 2, p
            else:
                assert not p['items'], p
        if 'Border colour' in declaration:
            assert all(p['presentation']['label'] == 'Border colour' for p in parameters), parameters

    shader.write_text("""
uniform float3 BadBounds < ui_min = "invalid"; ui_max = float2(1.0, 2.0); ui_step = 0.01; >;
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return float4(BadBounds, 1); }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
""")
    effect = read()[0]
    assert effect['error'] is None, effect
    assert [p['min'] for p in effect['parameters']] == [None, None, None], effect
    assert [p['max'] for p in effect['parameters']] == [1, 2, None], effect

    shader.write_text("""
uniform bool Action < ui_type = "button"; ui_label = "Capture frame"; > = false;
uniform float Locked < noedit = true; > = 1;
uniform float Keep < noreset = true; ui_label = "Keep value"; > = 0.5;
uniform bool3 Features < ui_label = "Features"; ui_items = "Depth\0\0Bloom\0"; >;
uniform int Mode < ui_type = "list"; ui_items = "First\0\0Third\0"; > = 2;
uniform float Blank < ui_label = "   "; >;
uniform float NotText < ui_label = 7; ui_tooltip = 4; >;
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return Action + Locked + Keep + Features.x + Mode + Blank + NotText; }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
""")
    effect = read()[0]
    assert effect['error'] is None, effect
    parameters = effect['parameters']
    assert len(parameters) == 7, parameters
    assert parameters[0]['label'] == 'Keep value' and parameters[0]['resettable'] is False
    assert [p['label'] for p in parameters[1:4]] == ['Features · Depth', 'Features [2]', 'Features · Bloom']
    assert all(p['presentation']['count'] == 1 and not p['items'] for p in parameters[1:4])
    assert parameters[4]['items'] == ['First', '', 'Third'] and parameters[4]['presentation']['widget'] == 'list'
    assert parameters[5]['label'] == 'Blank' and parameters[6]['label'] == 'NotText'
    assert parameters[6]['description'] == ''
    # Skipped controls cannot shift keys belonging to the remaining uniforms.
    assert parameters[0]['key'] == 'makoShader_41_4b656570_0'
