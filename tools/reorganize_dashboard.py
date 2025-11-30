import json
import os

DASHBOARD_PATH = r"e:\Repos\MyRepos\Knights\docker\observability\grafana\dashboards\server-metrics.json"

# Define the logical grouping of panels by their Title
# Panels not listed here will be added to an "Others" section at the bottom
SECTIONS = {
    "Overview": [
        "Active Sessions",
        "Accepted Connections",
        "Total Subscribes"
    ],
    "Performance": [
        "Dispatch Latency (ms)",
        "Dispatch Avg Latency",
        "Dispatch Latency Count",
        "Dispatch Latency Sum (ms)",
        "Subscribe Lag (ms)",
        "Frame Throughput (/s)",
        "Frame Payload Volume (/s)",
        "Frame Payload Size (bytes)",
        "Dispatch Throughput (/s)"
    ],
    "Internals": [
        "Job Queue Depth",
        "Memory Pool Usage",
        "Dispatch Opcode Totals",
        "Session Churn (/s)"
    ],
    "Errors": [
        "Dispatch Exceptions",
        "Timeouts & Drops (/s)",
        "Self Echo Drops Total",
        "Session Timeouts Total",
        "Heartbeat Timeouts Total",
        "Send Queue Drops Total"
    ],
    "Gateway": [
        "Gateway Active Sessions",
        "Gateway Total Connections"
    ],
    "Load Balancer": [
        "LB Active Backends",
        "LB Idle Close Total"
    ]
}

def create_row_panel(title, y_pos):
    return {
        "collapsed": False,
        "datasource": {
            "type": "datasource",
            "uid": "-- Mixed --"
        },
        "gridPos": {
            "h": 1,
            "w": 24,
            "x": 0,
            "y": y_pos
        },
        "id": 1000 + y_pos, # Simple ID generation
        "panels": [],
        "title": title,
        "type": "row"
    }

def create_panel(title, expr, y_pos, id_val):
    return {
        "datasource": {
            "type": "prometheus",
            "uid": "prometheus"
        },
        "gridPos": {
            "h": 8,
            "w": 12,
            "x": 0,
            "y": y_pos
        },
        "id": id_val,
        "targets": [
            {
                "expr": expr,
                "legendFormat": "{{instance}}",
                "refId": "A"
            }
        ],
        "title": title,
        "type": "timeseries"
    }

def reorganize():
    with open(DASHBOARD_PATH, 'r', encoding='utf-8') as f:
        dashboard = json.load(f)

    original_panels = dashboard.get('panels', [])
    new_panels = []
    
    # Map titles to panels for easy lookup
    panels_by_title = {}
    for p in original_panels:
        if p['type'] == 'row': continue # Skip existing rows
        panels_by_title[p['title']] = p

    # Inject missing panels
    if "Gateway Active Sessions" not in panels_by_title:
        panels_by_title["Gateway Active Sessions"] = create_panel("Gateway Active Sessions", "gateway_sessions_active", 0, 2001)
    if "Gateway Total Connections" not in panels_by_title:
        panels_by_title["Gateway Total Connections"] = create_panel("Gateway Total Connections", "gateway_connections_total", 0, 2002)
    if "LB Active Backends" not in panels_by_title:
        panels_by_title["LB Active Backends"] = create_panel("LB Active Backends", "lb_backends_active", 0, 2003)
    if "LB Idle Close Total" not in panels_by_title:
        # Reuse existing panel if it exists under old name or create new
        if "LB Idle Close Rate" in panels_by_title:
             panels_by_title["LB Idle Close Total"] = panels_by_title.pop("LB Idle Close Rate")
             panels_by_title["LB Idle Close Total"]["title"] = "LB Idle Close Total"
             panels_by_title["LB Idle Close Total"]["targets"][0]["expr"] = "lb_backend_idle_close_total"
        else:
             panels_by_title["LB Idle Close Total"] = create_panel("LB Idle Close Total", "lb_backend_idle_close_total", 0, 2004)

    current_y = 0
    
    # Process defined sections
    for section_name, panel_titles in SECTIONS.items():
        # Add Row
        row = create_row_panel(section_name, current_y)
        new_panels.append(row)
        current_y += 1
        
        # Add Panels in this section
        section_panels = []
        for title in panel_titles:
            if title in panels_by_title:
                section_panels.append(panels_by_title.pop(title))
        
        # Layout logic: 2 panels per row (width 12) or 4 small stats (width 6)
        # For simplicity, we'll keep original widths but just flow them
        current_x = 0
        row_height = 8 # Standard height
        
        for panel in section_panels:
            w = panel['gridPos']['w']
            h = panel['gridPos']['h']
            
            if current_x + w > 24:
                current_x = 0
                current_y += row_height
            
            panel['gridPos'] = {
                'h': h,
                'w': w,
                'x': current_x,
                'y': current_y
            }
            
            new_panels.append(panel)
            current_x += w
            
        # Move to next row after section
        current_y += row_height

    # Handle remaining panels
    if panels_by_title:
        row = create_row_panel("Others", current_y)
        new_panels.append(row)
        current_y += 1
        
        current_x = 0
        row_height = 8
        
        for title, panel in panels_by_title.items():
            w = panel['gridPos']['w']
            h = panel['gridPos']['h']
            
            if current_x + w > 24:
                current_x = 0
                current_y += row_height
            
            panel['gridPos'] = {
                'h': h,
                'w': w,
                'x': current_x,
                'y': current_y
            }
            
            new_panels.append(panel)
            current_x += w

    dashboard['panels'] = new_panels
    
    with open(DASHBOARD_PATH, 'w', encoding='utf-8') as f:
        json.dump(dashboard, f, indent=2)
    
    print("Dashboard reorganized successfully!")

if __name__ == "__main__":
    reorganize()
